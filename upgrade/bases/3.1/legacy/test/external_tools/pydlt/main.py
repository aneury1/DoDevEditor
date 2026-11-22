from pydlt import (
    ArgumentString,
    DltFileWriter,
    DltMessage,
    MessageLogInfo,
    MessageType,
    StorageHeader,
)

import random
import string

import os

def read_kernel_logs():
    log_files = ["/var/log/kern.log", "/var/log/syslog", "/var/log/dmesg"]  # Common log locations
    logs = []

    for log_file in log_files:
        if os.path.exists(log_file):
            with open(log_file, "r") as file:
                logs.extend(file.readlines())  # Read and store each line in an array
            break  # Stop after finding the first valid log file

    return logs

# Read kernel logs into an array
kernel_logs = read_kernel_logs()


 
# > 1970/01/01 00:00:00.000000 1 Ecu non-verbose [0] 010203
def random_three_letter_word():
    words=["KER","SYS","WIF","CON"]
    value = random.randint(1,3)
    return words[value]

def gen_dlt_message(payload):
    global value
    ecu = random_three_letter_word()
    apid = random_three_letter_word()
    ctid = random_three_letter_word()
    msg1 = DltMessage.create_verbose_message(
        [ArgumentString(payload)],
        MessageType.DLT_TYPE_LOG,
        MessageLogInfo.DLT_LOG_INFO,
        apid,
        ctid,
        message_counter=0,
        str_header=StorageHeader(0, 0, ecu),)
    return msg1



# Write DLT messages to file
with DltFileWriter("/home/neon/Desktop/test1000genv4.dlt") as writer:
    msgarrary=[]
    #for i in range(1,1000):
    #    msgarrary.append(gen_dlt_message())
    for i in kernel_logs:
         msgarrary.append(gen_dlt_message(i))
    writer.write_messages(msgarrary)
    #for i in range(1,1000):
    #    writer.write_messages([msg1, msg2])
