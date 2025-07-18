name := "GameOfLifeSpark"

version := "0.1"

scalaVersion := "2.13.12"

libraryDependencies ++= Seq(
  "org.apache.spark" %% "spark-core" % "4.0.0",
  "org.apache.spark" %% "spark-sql" % "4.0.0"
)

// Habilita o plugin de assembly
enablePlugins(AssemblyPlugin)

// Resolve conflitos de arquivos META-INF durante o empacotamento
assemblyMergeStrategy in assembly := {
  case PathList("META-INF", xs @ _*) => MergeStrategy.discard
  case x => MergeStrategy.first
}
