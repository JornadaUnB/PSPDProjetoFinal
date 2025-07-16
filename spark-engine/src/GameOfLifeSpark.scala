import org.apache.spark.{SparkConf, SparkContext}
import org.apache.spark.rdd.RDD
import java.io.FileWriter

object GameOfLifeSpark {
  // Constantes equivalentes às do código C
  val POWMIN = 3
  val POWMAX = 10
  
  def main(args: Array[String]): Unit = {
    val conf = new SparkConf().setAppName("GameOfLifeSpark")
    val sc = new SparkContext(conf)
    
    // Para cada tamanho de tabuleiro
    (POWMIN to POWMAX).foreach { pow =>
      val tam = 1 << pow
      val totalGenerations = 2 * (tam - 3)
      
      // Medição de tempo equivalente à versão C
      val t0 = System.nanoTime()
      
      // Inicialização do tabuleiro
      var currentGrid = initializeGrid(sc, tam)
      var nextGrid = sc.emptyRDD[(Int, Int, Int)]
      
      val t1 = System.nanoTime()
      
      // Execução das gerações
      for (_ <- 0 until totalGenerations) {
        nextGrid = computeNextGeneration(currentGrid, tam)
        currentGrid = nextGrid
      }
      
      val t2 = System.nanoTime()
      
      // Verificação do resultado (coletando para o driver)
      val finalGrid = currentGrid.collect()
      val isCorrect = checkResult(finalGrid, tam)
      
      val t3 = System.nanoTime()
      
      // Saída formatada similar ao código C
      println(if (isCorrect) "**Ok, RESULTADO CORRETO**" else "**Nok, RESULTADO ERRADO**")
      printf("tam=%d; tempos: init=%7.7f, comp=%7.7f, fim=%7.7f, tot=%7.7f \n",
        tam,
        (t1-t0)/1e9,
        (t2-t1)/1e9,
        (t3-t2)/1e9,
        (t3-t0)/1e9)
    }
    
    sc.stop()
  }
  
  // Inicializa o tabuleiro com o padrão do veleiro
  def initializeGrid(sc: SparkContext, tam: Int): RDD[(Int, Int, Int)] = {
    val aliveCells = Seq(
      (1, 2, 1), (2, 3, 1), (3, 1, 1), (3, 2, 1), (3, 3, 1)
    )
    
    // Cria um RDD com todas as células (a maioria mortas)
    val allCells = sc.parallelize(
      (0 to tam+1).flatMap(i => 
        (0 to tam+1).map(j => (i, j, 0))
      ).toSeq
    )
    
    // Atualiza com as células vivas
    allCells.map { case (i, j, _) =>
      aliveCells.find(c => c._1 == i && c._2 == j).getOrElse((i, j, 0))
    }
  }
  
  // Calcula a próxima geração
  def computeNextGeneration(grid: RDD[(Int, Int, Int)], tam: Int): RDD[(Int, Int, Int)] = {
    // Para cada célula, calcula seus vizinhos
    grid.flatMap { case (i, j, value) =>
      // Gera os 8 vizinhos mais a própria célula
      val neighbors = for {
        x <- i-1 to i+1
        y <- j-1 to j+1
        if x >= 0 && x <= tam+1 && y >= 0 && y <= tam+1
      } yield (x, y)
      
      // Emite a célula como vizinha de todos os seus vizinhos
      neighbors.map { case (x, y) =>
        ((x, y), (i, j, value))
      }
    }
    // Agrupa por coordenada da célula central
    .groupByKey()
    // Para cada célula, calcula o novo estado
    .map { case ((i, j), neighbors) =>
      val neighborList = neighbors.toList
      val currentValue = neighborList.find(_._1 == i).map(_._3).getOrElse(0)
      val livingNeighbors = neighborList.count { case (x, y, v) =>
        v == 1 && (x != i || y != j)
      }
      
      val newValue = currentValue match {
        case 1 if livingNeighbors < 2 => 0
        case 1 if livingNeighbors > 3 => 0
        case 0 if livingNeighbors == 3 => 1
        case _ => currentValue
      }
      
      (i, j, newValue)
    }
  }
  
  // Verifica se o resultado final está correto
  def checkResult(grid: Array[(Int, Int, Int)], tam: Int): Boolean = {
    val aliveCells = grid.filter(_._3 == 1).map { case (i, j, _) => (i, j) }
    
    aliveCells.length == 5 &&
    aliveCells.contains((tam-2, tam-1)) &&
    aliveCells.contains((tam-1, tam)) &&
    aliveCells.contains((tam, tam-2)) &&
    aliveCells.contains((tam, tam-1)) &&
    aliveCells.contains((tam, tam))
  }
}