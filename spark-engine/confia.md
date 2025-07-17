## Instale o sbt 
`sudo apt install sbt`

## compilar 

`cd spark-engine/`
`sbt package`

## Executar com o Spark

```
spark-submit \
  --class GameOfLifeSpark \
  --master local[*] \
  target/scala-2.12/gameoflifespark_2.12-0.1.jar 3 5
```