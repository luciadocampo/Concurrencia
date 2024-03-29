-module(break_md5).
-define(PASS_LEN, 6).
-define(UPDATE_BAR_GAP, 100000).
-define(BAR_SIZE, 40).
-define(NPROCS,6).

-export([break_md5/1,
         pass_to_num/1,
         num_to_pass/1,
         num_to_hex_string/1,
         hex_string_to_num/1,
         breakHashes/1
        ]).

-export([break_md5s/4,progress_loop/3]). % ahora es loop de 3 ya que se le pasan 3 parámetros.

% Base ^ Exp

pow_aux(_Base, Pow, 0) ->
    Pow;
pow_aux(Base, Pow, Exp) when Exp rem 2 == 0 ->
    pow_aux(Base*Base, Pow, Exp div 2);
pow_aux(Base, Pow, Exp) ->
    pow_aux(Base, Base * Pow, Exp - 1).

pow(Base, Exp) -> pow_aux(Base, 1, Exp).

%% Number to password and back conversion

num_to_pass_aux(_N, 0, Pass) -> Pass;
num_to_pass_aux(N, Digit, Pass) ->
    num_to_pass_aux(N div 26, Digit - 1, [$a + N rem 26 | Pass]).

num_to_pass(N) -> num_to_pass_aux(N, ?PASS_LEN, []).

pass_to_num(Pass) ->
    lists:foldl(fun (C, Num) -> Num * 26 + C - $a end, 0, Pass).

%% Hex string to Number

hex_char_to_int(N) ->
    if (N >= $0) and (N =< $9) -> N - $0;
       (N >= $a) and (N =< $f) -> N - $a + 10;
       (N >= $A) and (N =< $F) -> N - $A + 10;
       true                    -> throw({not_hex, [N]})
    end.

int_to_hex_char(N) ->
    if (N >= 0)  and (N < 10) -> $0 + N;
       (N >= 10) and (N < 16) -> $A + (N - 10);
       true                   -> throw({out_of_range, N})
    end.

hex_string_to_num(Hex_Str) ->
    lists:foldl(fun(Hex, Num) -> Num*16 + hex_char_to_int(Hex) end, 0, Hex_Str).

num_to_hex_string_aux(0, Str) -> Str;
num_to_hex_string_aux(N, Str) ->
    num_to_hex_string_aux(N div 16,
                          [int_to_hex_char(N rem 16) | Str]).

num_to_hex_string(0) -> "0";
num_to_hex_string(N) -> num_to_hex_string_aux(N, []).

%% Progress bar runs in its own process

progress_loop(N, Bound,T1) ->
    receive
        okey -> ok;
        {progress_report, Checked} ->
            N2 = N + Checked,
            Full_N = N2 * ?BAR_SIZE div Bound,
            Full = lists:duplicate(Full_N, $=),
            Empty = lists:duplicate(?BAR_SIZE - Full_N, $-),
            T2 = erlang:monotonic_time(micro_seconds),
            io:format("\r[~s~s] ~.2f% ~.6e/sec", [Full, Empty, N2/Bound*100, ?UPDATE_BAR_GAP/(T2-T1)*1000000]), % .6e porque 1 segundo contiene 1e+6 microsegundos entonces se toma esa medida con el monotonic_time
            progress_loop(N2, Bound,T2)
    end.

%% break_md5/2 iterates checking the possible passwords

break_md5([],_, _, _, _) -> ok;
break_md5(Target_Hash, N, N, _, _) -> {not_found, Target_Hash};  % Checked every possible password
break_md5(Target_Hash, N, Bound, Progress_Pid,Pid) ->
    if N rem ?UPDATE_BAR_GAP == 0 ->
            Progress_Pid ! {progress_report, ?UPDATE_BAR_GAP};
       true ->
            ok
    end,
    Pass = num_to_pass(N),
    Hash = crypto:hash(md5, Pass),
    Num_Hash = binary:decode_unsigned(Hash),

    case lists:member(Num_Hash,Target_Hash) of
        true-> io:format("\e[2K\r~.16B: ~s~n", [Num_Hash, Pass]),
            if length(Target_Hash) == 1 -> Pid ! okey; % cuando quede 1 contraseña , descrifra y para 
            true -> Pid ! {found_one,Num_Hash} 
        end;

        false -> ok

    end,
    receive
        {found_one,Num_hash} -> break_md5(lists:delete(Num_hash,Target_Hash),N+?NPROCS,Bound,Progress_Pid,Pid);
        okey->ok
        after(0) -> break_md5(Target_Hash,N+?NPROCS,Bound,Progress_Pid,Pid)
    end.

break_md5s(Hashes,Init,Pid,Progress_Pid) ->
    Bound = pow(26, ?PASS_LEN),
    List_Passwd = lists:map(fun hex_string_to_num/1, Hashes),
    Res = break_md5(List_Passwd, Init, Bound, Progress_Pid,Pid),
    Res.

wait_threads(Pid) ->
    receive
        {found_one,Num_Hash} -> [X!{found_one,Num_Hash} || X <- Pid], wait_threads(Pid);
        okey -> [X ! okey || X <- Pid]
    end.

start_threads(_,0,Pid,_) -> wait_threads(Pid);

start_threads(Hashes,N,Pid,Progress_Pid) -> List_Pid=[spawn (break_md5,break_md5s,[Hashes,N-1,self(),Progress_Pid]) | Pid],
                                            start_threads(Hashes,N-1,List_Pid,Progress_Pid).

%% Break a hash

breakHashes(Hashes) ->
    Bound = pow(26, ?PASS_LEN),
    T1 = erlang:monotonic_time(micro_seconds),   % aquí definimos la primera variable T1 la cual tomamos de referencia luego
    Progress_Pid = spawn(?MODULE, progress_loop, [0, Bound,T1]), % en progress_loop con T2 para medir la aproximación de hashes probados por segundo.
    start_threads(Hashes,?NPROCS,[],Progress_Pid),
    Progress_Pid ! okey,
    okey.

break_md5(Hash)->breakHashes([Hash]).
