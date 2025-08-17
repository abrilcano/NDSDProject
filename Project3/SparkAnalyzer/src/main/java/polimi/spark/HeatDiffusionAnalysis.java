package polimi.spark;

import static org.apache.spark.sql.functions.*;

import java.time.LocalTime;
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
                                .config("spark.sql.adaptive.enabled", "true")
                                .config("spark.sql.adaptive.coalescePartitions.enabled", "true")
                                .config("spark.sql.adaptive.skewJoin.enabled", "true")
                                .config("spark.serializer", "org.apache.spark.serializer.KryoSerializer")
                                .config("spark.sql.execution.arrow.pyspark.enabled", "true")
                                .getOrCreate();
                spark.sparkContext().setLogLevel("ERROR");

                System.out.println("SparkSession created with master " + master);
                System.out.println("Reading temperature data from " + filePath);

                // Read all temperature files into a single DataFrame
                Dataset<Row> df = spark.read()
                                .option("header", true) 
                                .option("inferSchema", true)
                                .option("multiLine", true)
                                .csv(filePath + "heat_diffusion*.csv")
                                .repartition(200);

                // Get time step column and transform it to timestamp
                df = df.withColumn("time_step", regexp_extract(input_file_name(), "heat_diffusion(\\d+)\\.csv", 1).cast("int"))
                        .withColumn("timestamp", from_unixtime(col("time_step")))
                        .repartition(col("x"), col("y")); // Partition by spatial coordinates for better locality

                df = df.cache(); // Cache before ordering to avoid recomputation

                // Show the schema and sample data 
                System.out.println("Schema of the temperature data:");
                df.printSchema();
                System.out.println("Sample data:");
                df.show(10);

                // System.out.println("Number of rows in the dataset: " + df.count());k

                // Write original dataset to csv (comment out for performance)
                // writeToCSV(df, "../results/", "original_data.csv");

                // Perform the queries
                
                computeMinMaxAvg(df);
                Dataset<Row> dfQ2 = computeWindowedDifference(df);
                computeMaxTemperatureDifference(dfQ2);

                // Stop the SparkSession
                spark.stop();
        }


        /**
         * Utility function to write DataFrame to CSV file
         */
        public static void writeToCSV(Dataset<Row> df, String outputPath, String fileName) {
                String fullPath = outputPath + fileName;
                System.out.println("Writing results to: " + fullPath);

                int numPartitions = Math.max(1, (int) Math.ceil(df.count() / 10000.0));
    
                df.repartition(Math.min(numPartitions, 10))
                                .write()
                                .mode("overwrite") // Overwrite if file exists
                                .option("header", "true") // Include column headers
                                .csv(fullPath);

                System.out.println("Successfully wrote results to " + fullPath);
        }

        /**
         * Query 1: For each point, compute the minimum, maximum, and average temperature difference over time.
         */
        public static void computeMinMaxAvg(Dataset<Row> df) {
                System.out.println("Query 1: Min, Max, and Average Temperature Difference Over Time");

                WindowSpec windowSpec = Window.partitionBy("x", "y").orderBy("time_step");

                Dataset<Row> result = df
                                .withColumn("prev_temperature", lag("temperature", 1).over(windowSpec))
                                .withColumn("temp_diff", col("temperature").minus(col("prev_temperature")))
                                .filter(col("prev_temperature").isNotNull())
                                .groupBy("x", "y")
                                .agg(
                                                min("temp_diff").as("min_temp_diff"),
                                                max("temp_diff").as("max_temp_diff"),
                                                avg("temp_diff").as("avg_temp_diff"))
                                .orderBy("x", "y");

                result.show(10);

                writeToCSV(result, "../results/", "query1_results.csv");
        }

        /**
         * Query 2: For each point, compute the difference in temperature over a time window of size 100Δt and slide 10Δt
         */
        public static Dataset<Row> computeWindowedDifference(Dataset<Row> df) {
                System.out.println("Query 2: Temperature Difference Over a Time Window");

                String windowSpec = "100 seconds";
                String slideSpec = "10 seconds";

                Dataset<Row> result = df
                        .groupBy(
                                col("x"),
                                col("y"),
                                window(col("timestamp"), windowSpec, slideSpec)
                        )
                        .agg(
                                first("temperature").as("temp_start"),
                                last("temperature").as("temp_end"),
                                min("timestamp").as("window_start"),
                                max("timestamp").as("window_end"),
                                min("time_step").as("window_start_step"),
                                max("time_step").as("window_end_step")
                        )
                        .withColumn("temp_diff", col("temp_end").minus(col("temp_start")))
                        .select(
                                col("x"),
                                col("y"),
                                col("window_start"),
                                col("window_end"),
                                col("window_start_step"),
                                col("window_end_step"),
                                col("temp_diff")
                        )
                        .orderBy("x", "y", "window_start_step", "window_end_step");

                result.show(20);

                writeToCSV(result, "../results/", "query2_results.csv");

                return result;
        }

        /**
         * Query 3: Compute the maximum temperature difference across all windows in
         * Query 2.
         */
        public static void computeMaxTemperatureDifference(Dataset<Row> df) {
                System.out.println("Query 3: Maximum Temperature Difference Across All Windows");

                // Now find the maximum temperature difference across all windows for each point
                Dataset<Row> result = df
                                .groupBy("x", "y")
                                .agg(max("temp_diff").as("max_temperature_diff"))
                                .orderBy("x", "y");

                result.show(10);

                // Write the results to CSV
                writeToCSV(result, "../results/", "query3_results.csv");
        }

}

