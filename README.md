# tcp-handshake
Testing tool for TCP handshake and high load data transmission.

This project is a client-server application that connects hosts in TCP mode. It imitates the behavior of [Wazuh](https://github.com/wazuh/wazuh): it performs a handshake to establish a connection in the user layer and starts a data transmission. The goal of this project is to server as a proof of concept for implementations in Wazuh.

## Throughput measurement

|Bytes / event|Gbps|Meps|
|---|---|---|
|8|0.472|30.744|
|16|0.679|22.256|
|32|1.033|10.640|
|64|1.568|5.397|
|128|2.742|3.105|
|256|3.392|1.828|
|512|9.038|2.248|
|1024|13.712|1.667|
|2048|13.271|0.809|
|4096|13.174|0.398|
|8192|13.481|0.205|
|16384|13.273|0.101|
|32768|13.408|0.051192|
|65536|14.436|0.027560|

![throughput](https://user-images.githubusercontent.com/10536251/49009970-330f3300-f172-11e8-8201-ac37a2374d11.png)

## Performance measurement

|Bytes / event|Gbps|Meps|
|---|---|---|
|8|0.29979|3.123|
|16|0.337768|2.111|
|32|0.361868|1.256|
|64|0.388227|0.713654|
|128|0.397512|0.376432|
|256|0.409292|0.196775|
|512|0.39917|0.096699|
|1024|0.404405|0.049174|
|2048|0.409132|0.024922|
|4096|0.413214|0.012598|
|8192|0.415498|0.006337|
|16384|0.412286|0.003145|
|32768|0.410422|0.001565|
|65536|0.410107|0.000782377|

![performance](https://user-images.githubusercontent.com/10536251/49009988-41f5e580-f172-11e8-9eff-0b0a5a652a7c.png)


