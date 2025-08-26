-module(comp).

-export([comp/1, comp/2 , decomp/1, decomp/2,comp_proc/2, comp_proc/3,decomp_proc/2,decomp_proc/3]).

-define(DEFAULT_CHUNK_SIZE, 1024*1024).

%%% File Compression

comp(File) -> %% Compress file to file.ch
    comp(File, ?DEFAULT_CHUNK_SIZE).

comp(File, Chunk_Size) ->  %% Starts a reader and a writer which run in separate processes
    case file_service:start_file_reader(File, Chunk_Size) of
        {ok, Reader} ->
            case archive:start_archive_writer(File++".ch") of
                {ok, Writer} ->
                    comp_loop(Reader, Writer);
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.

comp_proc(File, Procs) ->
    comp_proc(File, ?DEFAULT_CHUNK_SIZE, Procs).

comp_proc(File, Chunk_Size, Procs) ->
    case file_service:start_file_reader(File, Chunk_Size) of
        {ok, Reader} ->
            ArchiveFile = File ++ ".ch",
            case archive:start_archive_writer(ArchiveFile) of
                {ok, Writer} ->
                    spawn_multiple_compression_workers(Reader, Writer, Procs),
                    wait(Procs),
                    Reader !stop,
                    Writer ! stop,
                    {ok, ArchiveFile};
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason]),
                    file_service:stop_file_reader(Reader),
                    {error, Reason}
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason]),
            {error, Reason}
    end.

wait(0) ->
    ok;
wait(N) ->
    receive
        termine -> wait(N - 1)
    end.

spawn_multiple_compression_workers(_Reader, _Writer, 0) -> ok;
spawn_multiple_compression_workers(Reader, Writer, Procs) ->
    MyPid = self(),
    spawn_link(fun() -> comp_loop2(Reader, Writer, MyPid) end),
    spawn_multiple_compression_workers(Reader, Writer, Procs - 1).

comp_loop(Reader, Writer) ->  %% Compression loop => get a chunk, compress it, send to writer
    Reader ! {get_chunk, self()},  %% request a chunk from the file reader
    receive
        {chunk, Num, Offset, Data} ->   %% got one, compress and send to writer
            Comp_Data = compress:compress(Data),
            Writer ! {add_chunk, Num, Offset, Comp_Data},
            comp_loop(Reader, Writer);
        eof ->  %% end of file, stop reader and writer
            Writer !stop,
            Reader !stop;
        {error, Reason} ->
            io:format("Error reading input file: ~w~n",[Reason]),
            Reader ! stop,
            Writer ! abort
    end.

comp_loop2(Reader, Writer, Parent) ->  %% Compression loop => get a chunk, compress it, send to writer
    Reader ! {get_chunk, self()},  %% request a chunk from the file reader
    receive
        {chunk, Num, Offset, Data} ->   %% got one, compress and send to writer
            Comp_Data = compress:compress(Data),
            Writer ! {add_chunk, Num, Offset, Comp_Data},
            comp_loop2(Reader, Writer,Parent);
        eof ->  %% end of file, stop reader and writer
            Parent ! termine;
        {error, Reason} ->
            io:format("Error reading input file: ~w~n",[Reason]),
            Reader ! stop,
            Writer ! abort
    end.

%% File Decompression

decomp(Archive) ->
    decomp(Archive, string:replace(Archive, ".ch", "", trailing)).

decomp(Archive, Output_File) ->
    case archive:start_archive_reader(Archive) of
        {ok, Reader} ->
            case file_service:start_file_writer(Output_File) of
                {ok, Writer} ->
                    decomp_loop(Reader, Writer);
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason])
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason])
    end.

decomp_proc(Archive, Procs) ->
    decomp_proc(Archive, "", Procs).

decomp_proc(Archive, Output_File, Procs) ->
    case archive:start_archive_reader(Archive) of
        {ok, Reader} ->
            case file_service:start_file_writer(Output_File) of
                {ok, Writer} ->
                    spawn_multiple_decompression_workers(Reader, Writer, Procs),
                    wait(Procs),
                    Reader !stop,
                    Writer ! stop,
                    {ok, Output_File};
                {error, Reason} ->
                    io:format("Could not open output file: ~w~n", [Reason]),
                    file_service:stop_file_reader(Reader),
                    {error, Reason}
            end;
        {error, Reason} ->
            io:format("Could not open input file: ~w~n", [Reason]),
            {error, Reason}
    end.

spawn_multiple_decompression_workers(_Reader, _Writer, 0) -> ok;
spawn_multiple_decompression_workers(Reader, Writer, Procs) ->
     MyPid = self(),
    spawn_link(fun() -> decomp_loop2(Reader, Writer, MyPid) end),
    spawn_multiple_decompression_workers(Reader, Writer, Procs - 1).


decomp_loop(Reader, Writer) ->
    Reader ! {get_chunk, self()},  %% request a chunk from the reader
    receive
        {chunk, _Num, Offset, Comp_Data} ->  %% got one
            Data = compress:decompress(Comp_Data),
            Writer ! {write_chunk, Offset, Data},
            decomp_loop(Reader, Writer);
        eof ->    %% end of file => exit decompression
            Reader ! stop,
            Writer ! stop;
        {error, Reason} ->
            io:format("Error reading input file: ~w~n", [Reason]),
            Writer ! abort,
            Reader ! stop
    end.

decomp_loop2(Reader, Writer,Parent) ->
    Reader ! {get_chunk, self()},  %% request a chunk from the reader
    receive
        {chunk, _Num, Offset, Comp_Data} ->  %% got one
            Data = compress:decompress(Comp_Data),
            Writer ! {write_chunk, Offset, Data},
            decomp_loop2(Reader, Writer,Parent);
        eof ->    %% end of file => exit decompression
            Parent !termine;
        {error, Reason} ->
            io:format("Error reading input file: ~w~n", [Reason]),
            Writer ! abort,
            Reader ! stop
    end.
