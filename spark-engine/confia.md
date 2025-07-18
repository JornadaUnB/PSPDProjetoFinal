## 🛠️ Pré-requisitos

- Java JDK 17+
- Scala 2.13.x
- Apache Spark 4.0.0
- `sbt` (Scala Build Tool)
- Plugin `sbt-assembly` para empacotar o JAR completo

---

## 📁 Estrutura do Projeto

```text
spark-engine/
├── build.sbt
├── project/
│   └── plugins.sbt
└── src/
    └── main/
        └── scala/
            └── GameOfLifeSpark.scala
```
## Compilando o projeto

Dentro do spark-engine

`sbt clean assembly`

O JAR gerado estará em:

`target/scala-2.13/GameOfLifeSpark-assembly-0.1.jar`

Executando com o Spark,

Para rodar localmente com todos os núcleos disponíveis:

`
spark-submit   --class GameOfLifeSpark   --master local[*]   --conf "spark.driver.extraJavaOptions=-Dlog4j.configuration=file:log4j.properties"   spark-engine/target/scala-2.13/GameOfLifeSpark-assembly-0.1.jar
`


