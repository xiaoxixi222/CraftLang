$scoreboard objectives add $(main) dummy
scoreboard objectives add a dummy
scoreboard players set a a 0
execute store result score a a run scoreboard players get a a