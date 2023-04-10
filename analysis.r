library(ggplot2)

#LATENCY TEST

data_lcm <- read.csv("latency_results_full.csv")
data_lcm$type="LCM"

data_lcmsec <- read.csv("latency_results_full_secure.csv")
data_lcmsec$type="LCMsec"

data <- rbind(data_lcm, data_lcmsec)

# Create a boxplot of the latency values for each unique datasize value
latency_test <- ggplot(data, aes(x = datasize/1000))+
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
# print(latency_test)

#THROUGHPUT TEST
data_lcm <- read.csv("throughput_results.csv")
data_lcm$type="LCM"
data_lcmsec <- read.csv("throughput_results_secure.csv")
data_lcmsec$type="LCMsec"
data <- rbind(data_lcm, data_lcmsec)

# Create a boxplot of the latency values for each unique datasize value
p <- latency_test <- ggplot(data, aes(x = mbps, y=percent_received))+
    geom_point(aes(fill=type, color=type, shape=type, size=1.5))+
  scale_shape_manual(values = c("LCM" = 19, "LCMsec" = 17))+
  scale_color_manual(values = c("LCM" = "red", "LCMsec" = "blue")) +
  theme( legend.title=element_blank()) +
  theme(
    legend.position=c(0.15, 0.85),
    legend.text=element_text(size=12)
    )
plot(p)

# lcm <- read.csv("throughput_results.csv")
# plot(lcm$mbps, lcm$percent_received, 
#      xlab = "Attempted throughput (MB/s)", ylab = "percent of messages received", pch = 19 , col = "red")
#
# lcmsec <- read.csv("throughput_results_secure.csv")
# points(lcmsec$mbps, lcmsec$percent_received, pch = 17, col = "blue")
#
# # Connect the points with lines
# # lines(lcm$datasize/1000, lcm$avg_latency_us, col = "red")
# # lines(lcmsec$datasize/1000, lcmsec$avg_latency_us, col = "blue")
#
# # Add a legend to the plot
# legend("topleft", legend = c("LCM", "LCMsec"), pch = c(19, 17), col = c("red", "blue"))
#
# # Reset the layout to a single plot
# par(mfrow = c(1, 1))
