# MobileInsight Utils

`miutils` is a toolbox meant to be used together with the open-source software *MobileInsight* developed by the team founded by researchers from UCLA and Purdue (originally at OSU). (http://www.mobileinsight.net/)

It provides several functionalities to extract or filter the XML file generated by *MobileInsight* offline analyzer.

## Installation
Dependencies are as follows:

- C++14 compliant compiler
- Boost Library

Optional:

- x86 CPU with SSE and POPCNT feature (true since first gen of core i3)

To install, run the following commands. Root privilege may be needed for accessing `/usr/local/bin` while installing.
```
git clone https://github.com/Acciente717/miutils.git
cd miutils
make
make install
```

To uninstall, run
```
make uninstall
```

## Usage
`miutils [options] [input_files]`

When no input file is provided in the argument list, `miutils` reads from `stdin`. When no `-o` or `--output` option is set, it prints to `stdout`. Error and warning messages are always printed to `stderr`.

Multiple input files can be provided at a time. `miutils` will iteratively process them in the same sequence as they appear in the argument list.

The above behavior is analogous to that of `cat`, which makes it easy to logically concatenate multiple XML files and generate a single output file. Using `stdin` and `stdout` as the default I/O streams also makes it handy to read from and write to compressed gzip files on the fly by using chained pipes, i.e. `gzip -cd < src.gz | miutils | gzip -c > tgt.gz`.

To make it run, exactly one of the `extract`, `range`, `dedup`, `reorder` or `filter` mode must be set.

### `extract` Mode
Enable `extract` mode by setting `--extract extractor1,extractor2,...,extractorX`. Note that there is no space next to the commas.

`extract` mode extracts certain fields of certain types of packet from the XML file generated by *MobileInsight* offline analyzer. For each packet in the XML file, `miutils` iterates through the list of extractors. The first extractor which defines an action on that type of the packet will be invoked. All other extractors behind the invoked extractor will be ignored. In other words, if both extractors define actions on a certain type of packet, the one in the front in the extractor list will shadow the one in the back. Please consult the help message of `miutils` to find out all possible collisions.

### `range` Mode
Enable `range` mode by setting `--range range_file`.

`range` mode filters through the whole XML file. For each packet, it examines whether the timestamp of the packet falls in the ranges defined by the `range_file`. If it does, the packet will be printed to the output file, otherwise discarded.

`range_file` should be a text file. Each line in the file should denote a range in UNIX timestamp with precision in seconds, which looks like below:
```
start1 end1
start2 end2
start3 end3
...
```
The ranges are inclusive, and may overlap.

### `filter` Mode
Enable `filter` mode by setting `--filter reg_expr`.

`filter` mode keeps the packets to the output whose packet type matches the provided regular expression. The grammar of the regular expression should conform to ECMAScript.

### `reorder` Mode
Enable `reorder` mode by setting `--reorder window_size`.

`reorder` mode reorders the packets in the XML file according to the timestamp in each packets. This is a non-trivial task given that each input XML file may be as large as several tens of GBs. It is impossible to read in the whole XML and perform a full sort. `reorder` mode essentially performs a partial sort, but under certain assumption of the input XML file it will become a full sort.

`window_size` denotes a reordering window in microseconds. For each pair of packets X and Y, suppose the timestamp of X is smaller than that of Y, but Y occurs before X in the input XML file. If the difference of timestamps of Y and X are smaller than `window_size`, then X is guaranteed to appear before Y in the output. In other words, `--reorder` will fix the reverse order for the pairs of packets whose timestamps differences are smaller than `window_size`.

Based on experience, the original output XML file of *MobileInsight* offline analyzer contains small amount of reverse order packets. The timestamp difference between a pair of reversed order packets are usually small, typically no greater than tens of milliseconds. Setting `window_size` to be one second, i.e. `--reorder 1000000` is more than sufficient.

#### Caution
Even though the original output XML file of *MobileInsight* always contains some reverse order packets, it is *not always a good idea* to reorder them. For instance, PHY_PDSCH_STAT packets contain two fields indicating the frame and subframe numbers. Though the timestamp of these packets may not be monotonically increasing, the frame and subframe numbers are however in good sequential order, i.e. the timestamp is inconsistent with the frame and subframe numbers, while the latter is the correct one in some sense. Sorting the XML file according to timestamps will mess up the frame and subframe numbers. `--reorder` blows you up if you are reasoning based on frame and subframe numbers in this case.

### `dedup` Mode
Enable `dedup` mode by setting `--dedup`. Note that this mode should be used after `--reorder`.

`dedup` mode removes "duplicate" packets according to timestamps. For each packet in the input XML file, if its timestamp is no less than all previous packets, it will be printed to the output file, otherwize discarded.

`dedup` mode may seem bizarre at first glance. Indeed it is designed for special use cases. The current version of *MobileInsight* does have memory leak while running offline analyzers to convert binary LTE trace file to XML. For a binary LTE trace file of several GBs large, we must cut it into small chunks before sending them to *MobileInsight*. Otherwise, processing it as a whole will require hundreds of GBs of memory, or even more.

Naively cut binary files into chunks can lose packets on cutting boundaries. The graph below shows the case.

```
### No packet loss, but is extremely unlikely.
Before: ...[packet i][packet i+1][packet i+2][packet i+3]...
After:  ...[packet i][packet i+1] | CUT HERE | [packet i+2][packet i+3]...

### Has packet loss, very likely.
Before: ...[packet i][packet i+1][packet i+2][packet i+3]...
After:  ...[packet i][first half of packet i+1] | CUT IN THE MIDDLE |
           [second half of packet i+1][packet i+2][packet i+3]...
```

The cutting point is the most likely to land in the middle of a packet. There is no good way to avoid it, since we are cutting a binary file before parsing it into XML. When neither the chunk before the cutting point nor the one that after has the full information of packet i+1, *MobileInsight* simply discard the incomplete data. We thus miss that packet in the final XML files.

A feasible solution is to let neighboring chunks to overlap a small amount of data. `dedup` mode is thus to remove additional duplicated packets introduced by the overlapping data.

### Miscellaneous Options
`-h` or `--help` produces help messages.

`-o` or `--output` sets the output file rather than using `stdout`.

`-j` or `--thread` sets the working thread number. Default is 4.