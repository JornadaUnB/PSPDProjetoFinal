import sbtassembly.AssemblyPlugin.autoImport._
import sbtassembly.MergeStrategy
import sbtassembly.PathList

name := "GameOfLifeSpark"

version := "0.1"
scalaVersion := "2.13.12"

enablePlugins(AssemblyPlugin)

ThisBuild / mainClass := Some("GameOfLifeSpark")
ThisBuild / libraryDependencySchemes += "com.github.luben" % "zstd-jni" % "always"

libraryDependencies ++= Seq(
  "org.apache.spark" %% "spark-core" % "4.0.0",
  "org.apache.spark" %% "spark-sql" % "4.0.0",
  "org.apache.kafka" %% "kafka" % "2.8.0",
  "org.apache.kafka" % "kafka-clients" % "2.8.0"
)

assemblyMergeStrategy in assembly := {
  case PathList("META-INF", xs @ _*) => MergeStrategy.discard
  case x => MergeStrategy.first
}
