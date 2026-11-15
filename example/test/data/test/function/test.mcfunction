say 123456
say "this is a test"

scoreboard objectives remove sb
scoreboard objectives add sb dummy
scoreboard players set p sb 10
scoreboard players add p sb 5
scoreboard players remove p sb 3
scoreboard players set q sb 2
scoreboard players operation p sb *= q sb
scoreboard players set t sb 24
scoreboard players operation t sb += q sb
scoreboard players operation t sb -= q sb
scoreboard players operation t sb *= q sb
scoreboard players operation t sb /= q sb
scoreboard players operation t sb %= q sb

execute store result storage test 0 int 1 run scoreboard players get p sb
function test:callee with storage test
execute store result storage test 0 int 1 run scoreboard players get t sb
function test:callee with storage test
function test:plain
execute run say execute-run-ok

execute if score p sb = q sb run say if-eq-bad
execute unless score p sb = q sb run say unless-ne-ok
execute if score p sb >= q sb run say if-ge-ok
execute if score p sb matches 20..30 run say if-range-ok
execute unless score p sb matches 30..40 run say unless-range-ok

scoreboard players set zero sb 0
scoreboard players set r sb 7
scoreboard players operation r sb /= zero sb
say div-zero-continued
execute store result storage test 0 int 1 run scoreboard players get r sb
function test:callee with storage test

scoreboard players add noscore sb 1
say missing-score-continued
execute store result storage test 0 int 1 run scoreboard players get noscore sb
function test:callee with storage test

scoreboard players set p missingobj 1
say missing-objective-continued

function test:callee
say missing-source-continued

execute store result score p sb int 1 run scoreboard players get p sb
say store-score-rejected

execute if score p sb matches 1.. run return 0
say should-not-print
