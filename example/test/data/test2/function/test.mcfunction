$scoreboard objectives add $(main) dummy
scoreboard objectives add b dummy
scoreboard players set b b 0
execute store result score b b run scoreboard players get b b