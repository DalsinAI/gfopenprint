/* ViewerInfo.rexx: what GFOpenView shows now.
   rx ViewerInfo.rexx   (GFOpenView running) */
OPTIONS RESULTS
IF ~SHOW('P', 'GFOPENVIEW') THEN DO
    SAY 'GFOpenView is not running.'
    EXIT 5
END
ADDRESS 'GFOPENVIEW'
'VERSION'
SAY RESULT
'GETFILEINFO'
SAY 'Showing:' RESULT
'ACTIVATE'
