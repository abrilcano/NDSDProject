package polimi.spark;

import static org.apache.spark.sql.functions.*;

import org.apache.spark.sql.Dataset;
import org.apache.spark.sql.Row;
import org.apache.spark.sql.SparkSession;
import org.apache.spark.sql.expressions.Window;
import org.apache.spark.sql.expressions.WindowSpec;


public class HeatDiffusionAnalysis {

    public static void main(String[] args) {
        // Initialize SparkSession
        final String master = args.length > 0 ? args[0] : "local[*]";
        // Path to the directory containing temperature files
        String filePath = args.length > 1 ? args[1] : "../output/";
        
        final SparkSession spark = SparkSession.builder()
        .master(master)
        .appName("HeatDiffusionAnalysis")
        .getOrCreate();
        spark.sparkContext().setLogLevel("ERROR");
        
        System.out.println("SparkSession created with master " + master);
        System.out.println("Reading temperature data from " + filePath);

        // Read all temperature files into a single DataFrame
        Dataset<Row> df = spark.read()
                .option("header", true) // First line is the header
                .option("inferSchema", true) // Infer data types
                .csv(filePath + "heat_diffusion*.txt");
        
        df = df.withColumn("time_step", monotonically_increasing_id());

        // Show the schema and sample data
        System.out.println("Schema of the temperature data:");
        df.printSchema();
        System.out.println("Sample data:");
        df.show(10);

        System.out.println("Number of rows in the dataset: " + df.count());

        // Perform the queries
        // computeMinMaxAvg(df);
        // computeWindowedDifference(df);
        // computeMaxTimeDifference(df);

        // Stop the SparkSession
        spark.stop();
    }

        /**
     * Query 1: Compute the minimum, maximum, and average temperature for each point.
     */
    public static void computeMinMaxAvg(Dataset<Row> df) {
        System.out.println("Query 1: Min, Max, and Average Temperature for Each Point");

        Dataset<Row> result = df.groupBy("x", "y")
                .agg(
                        min("temperature").as("min_temperature"),
                        max("temperature").as("max_temperature"),
                        avg("temperature").as("avg_temperature")
                );

        result.show();
    }

    /**
     * Query 2: Compute the temperature difference over a time window of size 100Δt and slide 10Δt.
     */
    public static void computeWindowedDifference(Dataset<Row> df) {
        System.out.println("Query 2: Temperature Difference Over a Time Window");

        // Define a window specification
        WindowSpec windowSpec = Window.partitionBy("x", "y")
                .orderBy("time_step")
                .rowsBetween(-100, 0); // Window size of 100 time steps

        // Compute the temperature difference over the window
        Dataset<Row> result = df.withColumn("temperature_diff",
                col("temperature").minus(lag("temperature", 100).over(windowSpec)))
                .filter(col("temperature_diff").isNotNull()); // Filter out null values

        result.show();
    }

    /**
     * Query 3: Compute the maximum time difference across all windows in Query 2.
     */
    public static void computeMaxTimeDifference(Dataset<Row> df) {
        System.out.println("Query 3: Maximum Time Difference Across All Windows");

        // Define a window specification
        WindowSpec windowSpec = Window.partitionBy("x", "y")
                .orderBy("time_step")
                .rowsBetween(-100, 0); // Window size of 100 time steps

        // Compute the temperature difference over the window
        Dataset<Row> windowedDiff = df.withColumn("temperature_diff",
                col("temperature").minus(lag("temperature", 100).over(windowSpec)))
                .filter(col("temperature_diff").isNotNull()); // Filter out null values

        // Compute the maximum time difference for each point
        Dataset<Row> result = windowedDiff.groupBy("x", "y")
                .agg(max("temperature_diff").as("max_temperature_diff"));

        result.show();
    }

}
