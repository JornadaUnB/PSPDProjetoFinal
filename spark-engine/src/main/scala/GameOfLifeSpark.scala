import java.time.Instant
import org.apache.kafka.clients.consumer.{KafkaConsumer, ConsumerConfig}
import java.util.{Properties, Collections}
import scala.jdk.CollectionConverters._

object GameOfLifeSpark {

  def ind2d(i: Int, j: Int, tam: Int): Int = i * (tam + 2) + j

  def wallTime(): Double = Instant.now.toEpochMilli / 1000.0

  def umaVida(tabulIn: Array[Int], tabulOut: Array[Int], tam: Int): Unit = {
    for (i <- 1 to tam) {
      for (j <- 1 to tam) {
        val vizviv =
          tabulIn(ind2d(i - 1, j - 1, tam)) +
          tabulIn(ind2d(i - 1, j, tam)) +
          tabulIn(ind2d(i - 1, j + 1, tam)) +
          tabulIn(ind2d(i, j - 1, tam)) +
          tabulIn(ind2d(i, j + 1, tam)) +
          tabulIn(ind2d(i + 1, j - 1, tam)) +
          tabulIn(ind2d(i + 1, j, tam)) +
          tabulIn(ind2d(i + 1, j + 1, tam))

        val idx = ind2d(i, j, tam)
        tabulOut(idx) = tabulIn(idx) match {
          case 1 if vizviv < 2 || vizviv > 3 => 0
          case 0 if vizviv == 3 => 1
          case current => current
        }
      }
    }
  }

  def initTabul(tabulIn: Array[Int], tabulOut: Array[Int], tam: Int): Unit = {
    java.util.Arrays.fill(tabulIn, 0)
    java.util.Arrays.fill(tabulOut, 0)

    tabulIn(ind2d(1,2,tam)) = 1
    tabulIn(ind2d(2,3,tam)) = 1
    tabulIn(ind2d(3,1,tam)) = 1
    tabulIn(ind2d(3,2,tam)) = 1
    tabulIn(ind2d(3,3,tam)) = 1
  }

  def correto(tabul: Array[Int], tam: Int): Boolean = {
    val total = tabul.sum
    total == 5 &&
      tabul(ind2d(tam - 2, tam - 1, tam)) == 1 &&
      tabul(ind2d(tam - 1, tam, tam)) == 1 &&
      tabul(ind2d(tam, tam - 2, tam)) == 1 &&
      tabul(ind2d(tam, tam - 1, tam)) == 1 &&
      tabul(ind2d(tam, tam, tam)) == 1
  }

  def createKafkaConsumer(): KafkaConsumer[String, String] = {
    val props = new Properties()
    props.put(ConsumerConfig.BOOTSTRAP_SERVERS_CONFIG, "kafka:9092")
    props.put(ConsumerConfig.GROUP_ID_CONFIG, "spark-topic")
    props.put(ConsumerConfig.KEY_DESERIALIZER_CLASS_CONFIG, "org.apache.kafka.common.serialization.StringDeserializer")
    props.put(ConsumerConfig.VALUE_DESERIALIZER_CLASS_CONFIG, "org.apache.kafka.common.serialization.StringDeserializer")
    props.put(ConsumerConfig.AUTO_OFFSET_RESET_CONFIG, "earliest")
    new KafkaConsumer[String, String](props)
  }

  def main(args: Array[String]): Unit = {
    val consumer = createKafkaConsumer()
    consumer.subscribe(Collections.singletonList("game-of-life-config"))
    println("👂 Aguardando mensagens Kafka no tópico 'game-of-life-config'...")

    while (true) {
      val records = consumer.poll(java.time.Duration.ofSeconds(5))
      for (record <- records.asScala) {
        val value = record.value()
        println(s"📝 Mensagem recebida: $value")

        val parts = value.split(",").map(_.trim)

        if (parts.length != 2) {
          println(s"⚠️ Ignorando mensagem malformada: '$value'")
        } else {
          try {
            val powMin = parts(0).toInt
            val powMax = parts(1).toInt

            for (pow <- powMin to powMax) {
              val tam = 1 << pow
              val size = (tam + 2) * (tam + 2)

              val t0 = wallTime()
              val tabulIn = Array.ofDim[Int](size)
              val tabulOut = Array.ofDim[Int](size)

              initTabul(tabulIn, tabulOut, tam)
              val t1 = wallTime()

              for (_ <- 0 until 2 * (tam - 3)) {
                umaVida(tabulIn, tabulOut, tam)
                umaVida(tabulOut, tabulIn, tam)
              }

              val t2 = wallTime()

              if (correto(tabulIn, tam)) println("✅ RESULTADO CORRETO")
              else println("❌ Nok, RESULTADO ERRADO")

              val t3 = wallTime()

              println(f"[POW=$pow | tam=$tam] tempos: init=${t1 - t0}%.7f, comp=${t2 - t1}%.7f, fim=${t3 - t2}%.7f, tot=${t3 - t0}%.7f\n")
            }
          } catch {
            case e: NumberFormatException =>
              println(s"⚠️ Valores inválidos na mensagem: '$value'")
          }
        }
      }
    }
  }
}
