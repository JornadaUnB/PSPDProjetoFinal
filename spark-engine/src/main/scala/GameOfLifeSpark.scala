import org.apache.spark.sql.SparkSession

object GameOfLifeSpark {
  def main(args: Array[String]): Unit = {
    if (args.length != 2) {
      println("Uso: GameOfLifeSpark <POWMIN> <POWMAX>")
      sys.exit(1)
    }

    val powMin = args(0).toInt
    val powMax = args(1).toInt

    val spark = SparkSession.builder
      .appName("Game of Life Spark")
      .master("local[*]")
      .getOrCreate()

    val sc = spark.sparkContext

    for (pow <- powMin to powMax) {
      val tam = 1 << pow
      val board = Array.ofDim[Int](tam, tam)

      // Inicialização do "veleiro"
      board(1)(2) = 1
      board(2)(3) = 1
      board(3)(1) = 1
      board(3)(2) = 1
      board(3)(3) = 1

      val start = System.nanoTime()

      var current = board.map(_.clone)
      for (_ <- 0 until 2 * (tam - 3)) {
        val rdd = sc.parallelize(current.zipWithIndex)

        val updated = rdd.map { case (row, i) =>
          row.indices.map { j =>
            val neighbors = for {
              di <- -1 to 1
              dj <- -1 to 1
              if (di != 0 || dj != 0)
              ni = i + di
              nj = j + dj
              if ni >= 0 && ni < tam && nj >= 0 && nj < tam
            } yield current(ni)(nj)

            val alive = neighbors.count(_ == 1)
            if (current(i)(j) == 1 && (alive < 2 || alive > 3)) 0
            else if (current(i)(j) == 0 && alive == 3) 1
            else current(i)(j)
          }.toArray
        }

        current = updated.collect()
      }

      val end = System.nanoTime()
      val duration = (end - start) / 1e9

      println(s"Tamanho $tam finalizado em $duration segundos")

      // Verificação simples do "veleiro" no final
      val success = List(
        current(tam - 3)(tam - 2),
        current(tam - 2)(tam - 1),
        current(tam - 1)(tam - 3),
        current(tam - 1)(tam - 2),
        current(tam - 1)(tam - 1)
      ).count(_ == 1) == 5

      if (success) println("**Ok, RESULTADO CORRETO**")
      else println("**Nok, RESULTADO ERRADO**")
    }

    spark.stop()
  }
}
