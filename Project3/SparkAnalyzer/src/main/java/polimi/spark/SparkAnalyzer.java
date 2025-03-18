import org.apache.spark.sql.Dataset;
import org.apache.spark.sql.Row;
import org.apache.spark.sql.SparkSession;

public class SparkAnalyzer {

    public static void main(String[] args) {
        final String master = "local[*]";
        final String filePath = "src/main/resources/heat_diffusion_*.csv"; // Now processing CSV files

        final SparkSession spark = SparkSession
                .builder()
                .master(master)
                .appName("SparkAnalyzer")
                .getOrCreate();

        Dataset<Row> df = spark
                .readStream()
                .option("header", "true")
                .option("inferSchema", "true")
                .csv(filePath);
    }
}