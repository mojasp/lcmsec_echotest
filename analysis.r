#  draw already avgd data

# # Read the first csv file into a data frame and plot
# data1 <- read.csv("latency_results.csv")
# plot(data1$datasize/1000, data1$avg_latency_us, 
#      xlab = "Data Size (KB)", ylab = "Average Latency (us)", pch = 19, log = "xy", col = "red")
#
# # Read the second csv file into a data frame and add to the plot
# data2 <- read.csv("latency_results_secure.csv")
# points(data2$datasize/1000, data2$avg_latency_us, pch = 17, col = "blue")
#
# # Connect the points with lines
# lines(data1$datasize/1000, data1$avg_latency_us, col = "blue")
# lines(data2$datasize/1000, data2$avg_latency_us, col = "red")
#
# # Add a legend to the plot
# legend("topleft", legend = c("LCM", "LCMsec"), pch = c(19, 17), col = c("red", "blue"))
#
# # Reset the layout to a single plot
# par(mfrow = c(1, 1))


# install.packages("reshape2")
# install.packages('tidyr')
library(reshape2)
library(ggplot2)

data_lcm <- read.csv("latency_results_full.csv")
data_lcm$type="LCM"

data_lcmsec <- read.csv("latency_results_full_secure.csv")
data_lcmsec$type="LCMsec"

data <- rbind(data_lcm, data_lcmsec)

# Create a boxplot of the latency values for each unique datasize value
p <- ggplot(data, aes(x = datasize/1000))+
  geom_boxplot(aes(y=latency, group=interaction(datasize, type), fill=type), outlier.shape = NA) +
  scale_x_log10(name = "Data Size (kilobytes)") +
  scale_y_log10(breaks=c(12, 15, 20,30,50,100,150,300)) + #change this according to results to look good
  # scale_y_continuous(breaks=pretty(data$latency, n=10))+
  scale_fill_manual(values = c("LCM" = "red", "LCMsec" = "blue"))+
  labs(y = "RTT Lateny (microseconds)") +
  theme( legend.title=element_blank()) +
  theme(
    legend.position=c(0.15, 0.85),
    legend.text=element_text(size=12)
    )

print(p)

