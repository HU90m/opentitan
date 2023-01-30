# User Reference

## Issuing a Controller Read

To issue a flash read, the programmer must
*  Specify the address of the first flash word to read
*  Specify the number of total flash words to read, beginning at the supplied address
*  Specify the operation to be 'READ' type
*  Set the 'START' bit for the operation to begin

The above fields can be set in the {{< regref "CONTROL" >}} and {{< regref "ADDR" >}} registers.
See [library code](https://github.com/lowRISC/opentitan/blob/master/sw/device/lib/flash_ctrl.c) for implementation.

It is acceptable for total number of flash words to be significantly greater than the depth of the read FIFO.
In this situation, the read FIFO will fill up (or hit programmable fill value), pause the flash read and trigger an interrupt to software.
Once there is space inside the FIFO, the controller will resume reading until the appropriate number of words have been read.
Once the total count has been reached, the flash controller will post OP_DONE in the {{< regref "OP_STATUS" >}} register.

## Issuing a Controller Program

To program flash, the same procedure as read is followed.
However, instead of setting the {{< regref "CONTROL" >}} register for read operation, a program operation is selected instead.
Software will then fill the program FIFO and wait for the controller to consume this data.
Similar to the read case, the controller will automatically stall when there is insufficient data in the FIFO.
When all desired words have been programmed, the controller will post OP_DONE in the {{< regref "OP_STATUS" >}} register.

## Debugging a Read Error
Since flash has multiple access modes, debugging read errors can be complicated.
The following lays out the expected cases.

### Error Encountered by Software Direct Read
If software reads the flash directly, it may encounter a variety of errors (read data integrity / ECC failures, both reliability and integrity).
ECC failures create in-band error responses and should be recognized as a bus exception.
Read data integrity failures also create exceptions directly inside the processor as part of end-to-end transmission integrity.

From these exceptions, software should be able to determine the error address through processor specific means.
Once the address is discovered, further steps can be taken to triage the issue.

### Error Encountered by Software Initiated Controller Operations
A controller operation can encounter a much greater variety of errors, see {{< regref "ERR_CODE" >}}.
When such an error is encountered, as reflected by {{< regref "OP_STATUS" >}} when the operation is complete, software can examine the {{< regref "ERR_ADDR" >}} to determine the error location.
Once the address is discovered, further steps can be taken to triage the issue.

### Correctable ECC Errors
Correctable ECC errors are by nature not fatal errors and do not stop operation.
Instead, if the error is correctable, the flash controller fixes the issue and registers the last address where a single bit error was seen.
See {{< regref "ECC_SINGLE_ERR_CNT" >}} and {{< regref "ECC_SINGLE_ERR_ADDR" >}}

## Device Interface Functions (DIFs)

{{< dif_listing "sw/device/lib/dif/dif_flash_ctrl.h" >}}

## Register Table

The flash protocol controller maintains two separate access windows for the FIFO.
It is implemented this way because the access window supports transaction back-pressure should the FIFO become full (in case of write) or empty (in case of read).

{{< incGenFromIpDesc "../data/flash_ctrl.hjson" "registers" >}}
