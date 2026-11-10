/* tests/arexx.rexx: GFOpenView's ARexx port against ARexx-RULE.md
   (return codes 0/5/10/20, no requester holds up a command).
   Start GFOpenView from a Shell with no file (run GFOpenView), then
   rx tests/arexx.rexx [picture]   (prints PASS/FAIL lines, then ALL PASS or n FAILED).
   That the script answers at all shows no file requester is holding the port. */
OPTIONS RESULTS
PARSE ARG picture
IF picture = '' THEN picture = 'SYS:Prefs/Presets/Pointers/Busy.pointer'
port = 'GFOPENVIEW'
IF ~SHOW('P', port) THEN DO
    SAY 'FAIL' port 'is not there: start GFOpenView first'
    EXIT 20
END
ADDRESS VALUE port
fails = 0

CALL want 'VERSION', 0
CALL want 'HELP', 0
CALL want 'FAULT 205', 0
CALL want 'NOSUCHCOMMAND', 20
CALL want 'LASTERROR', 0
IF res = '' THEN CALL bad 'LASTERROR after an unknown command is empty'
ELSE SAY '     LASTERROR:' res
CALL want 'OPEN', 10                        /* the window's Open asks; a script's doesn't */
CALL want 'GETOBJECTINFO', 10               /* nothing open */
CALL want 'SET', 10
CALL want 'COPY', 5
CALL want 'OPEN RAM:no/such/file.iff', 10
CALL want 'LASTERROR', 0
SAY '     LASTERROR:' res
IF EXISTS(picture) THEN DO
    CALL want 'OPEN NAME' picture, 0
    CALL want 'GETFILEINFO', 0
END
ELSE SAY 'SKIP' picture 'is not there'
CALL want 'DEACTIVATE', 5                   /* no icon to go to */
CALL want 'ACTIVATE', 0
CALL want 'QUIT', 0

IF fails = 0 THEN SAY 'ALL PASS'
ELSE SAY fails 'FAILED'
EXIT fails

want: PROCEDURE EXPOSE fails res
    PARSE ARG cmd, rcwant
    res = ''
    cmd
    got = rc
    IF got = 0 THEN res = result
    IF got = rcwant THEN SAY 'PASS' cmd 'RC' got
    ELSE CALL bad cmd 'RC' got '(wanted' rcwant')'
    RETURN

bad:
    PARSE ARG why
    SAY 'FAIL' why
    fails = fails + 1
    RETURN
