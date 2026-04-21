# 在线调度

太长不读: 在线调度问题。

## 问题概述

现在， 你有一个 CPU 集群和若干任务，每个 CPU 的配置都是一样的（即单独执行相同任务所需要的时间相同），你需要对 CPU 集群进行调度， 以便在给定的时间内完成尽可能多的任务。 你调度的粒度是一个时间片，而在每个时间片中， 一个 CPU 上只能执行一个任务。假设我们用 $m$ 个任务 $a_1,\cdots,a_m$。每个任务 $a_i$ 有一个给定的负载 $w_i$ 和一个给定的时间需求（DDL）$t_i$，以及一个由用户指定的优先级 $p_i$，你需要最大化 SLO (Service Level Objective) 满足率。假设你完成的任务序号为 $y_1,y_2,\cdots,y_n$，那么 SLO 满足率的计算方式如下：
$$
SLO=\frac{\sum_{i=1}^{n}{p_{y_i}}}{\sum_{i=1}^{m}{p_i}}
$$
对于每个任务， 其可以在任意一个 CPU 上执行， 但是**一旦开始执行，不能重新启动，直到保存完成， 才会计入贡献**。 需要注意， 一个任务的启动有一个 `kStartUp` 冷启动时间，即如果这个 CPU 之前没有在执行该任务，一定要消费 `kStartUp` 的时间用于加载任务等预处理工作； 在保存的时候也有 `kSaving` 的保存时间，即在决定开始保存之后还需要额外的 `kSaving` 才能完成存储模型等工作。而实际运行时间计算的则是从冷启动成功到开始保存的这一段时间。以下给出一个任务的执行流程:

- `t0`: 启动某个任务。
- `t0 + kStartUp`: 任务开始执行。
- `t1`: 开始保存任务。
- `t1 + kSaving`: 任务保存完成。

一个流程是合法的， 当且仅当:

- `t0 + kStartUp <= t1`
- `t1 + kSaving <= deadline`

且计入的贡献部分是 `t1 - t0 - kStartUp`。 且在 `t0, t0 + 1, ... t1 - 1` 这段时间内， 任务都是在执行的， 即会占用 CPU 核心，该 CPU 核心无法被用于其他用途，如果你错误地试图将其用于调度其他任务，会被视为**所有 scheduling 操作无效**。

同时，你也可以**取消**一组任务，即使得当前的所有正在执行该任务的 CPU 变得立即空闲，可以在**当时刻**内开始启动新的任务，但是自上次保存开始后的所有进度 **作废**。而在评测的时候，我们在每个给定的时间内会先执行你提交的所有取消操作，接着执行所有的保存操作，最后是所有启动操作。

事实上， 你可以将一个任务分配给多个 CPU 同时执行， 但是加速比 (有效核心数量，正比于完成任务的“速度”) 并不是线性的。 具体请看 `definition.h` 里面的 `time_policy` 函数。 举例来说， 假如有 k 个核心， 从启动到保存的时间差是 `t`， 那么加速比是 `pow(k, c) * (t - kStartUp)`， 其中 `c` 是一个常数。

### Generator side

你需要实现一个生成器， 具体函数签名参考 `interface.h`。

其中， 我们要求你返回一个任务列表， 且满足传入的 Description。 如果生成的列表不满足 Description， 则认为 generate 失败。

Description 包括:

- cpu 核心数量 (题目中给出, 均为 114)
- task_count 任务数量
- deadline_time 任务的 deadline 时间区间
- execute_time_single 单个任务执行时长的区间
- execute_time_sum 所有任务执行总时长的区间
- priority_single 单个任务优先级的区间
- priority_sum 所有任务优先级总和的区间

### Scheduler side

你需要实现一个调度器， 具体函数签名参考 `interface.h`。

其中， 我们提供了每个周期内新增的任务列表， 任务编号 (task_id) 默认从 0 开始。 但是，

你需要返回一个调度方案， 其中包括每个任务的执行时间段， 以及执行的 CPU 核心数量。

请注意， cpu 核心数有限， 如果超额使用直接认为 schedule 失败。 同样， 如果 task_id 超出当前给出的任务范围， 或者在做出非法调度 (比如在某个任务在 Saving 或者 Launch 的时候继续 Launch， 或者在不是 Launch 的时候 Saving)， 也认为 schedule 失败。

## 评分

我们将会把所有人生成的 scheduling plan 和 data 放在一起测试，对于每一组调度方和数据生成方（以下简记为防守方、进攻方）：记防守方在此轮数据中的 SLO 满足率为 $x$，进攻方对自己生成的数据的满足率为 $y$，那么防守方在此轮中的得分为 $\text{min}\{\frac 1 2 \times \frac{x}{y},1\}$.

如果进攻方生成的数据不满足要求，或者进攻方对于自己生成的数据生成的调度非法，或者 $y = 0$，那么防守方直接获得 $1$ 分。

在进攻方生成的数据合法， $y \ne 0$ 的基础上，如果防守方生成的调度不满足要求，那么防守方得 $0$ 分。

## 其他提示

### OJ

#### 下发文件

我们下发与 OJ 几乎一致的相关文件。

`interface.h` 规定了你要实现的函数。

`definition.h` 中，有所有类型的定义，以及我们 oj 上的测试点相关的 Description 信息 (testcast_array)。同时，我们也把部分常数信息放在了 `PublicInformation` 类里面，你可以在你的代码中使用这些信息。

`runtime.h` 是我们 OJ 运行时用到的模拟器相关代码。你可以观察 `runtime.h` 中的 `RuntimeManager` 来了解我们 OJ 具体测评相关的细节。

`src.hpp` 是你需要实现对应接口的头文件。

`client.cpp`, `server.cpp`, `client_spj.cpp`, `server_spj.cpp` 是我们 OJ 测评用到的文件，你不应该修改这些文件。

我们的测评流程是:

从 stdin 读入 `0 ~ 3` 中某个数字 -> `client.cpp` -> `client_spj.cpp` -> `server.cpp` -> `server_spj.cpp`

在本地测评的时候，只需要使用 `run.sh` 即可。

#### 关于 gcc 版本

我们要求 gcc 的版本至少是 11， 编译参数请参考 `run.sh`。 如果你要安装 gcc 11 的话，请参考：

```shell
lsb_release -a # 查看你的 ubuntu 版本
# for ubuntu 20.04
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-11 g++-11
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 60 --slave /usr/bin/g++ g++ /usr/bin/g++-11

# for ubuntu 22.04
gcc --version # 如果这里已经显示是 11.*，就好了
sudo apt update
sudo apt install build-essential
gcc --version

# 运行程序
cd oj
sh run.sh
# 输入 0 ~ 3 的数字即可运行对应的测试点
```

#### 关于本地评测和 OJ 提交

你可以在命令行输入 `sh run.sh` 来测试你的 `generator` 和 `scheduler` 的正确性。

在上传到 OJ 的时候， 你需要将 `src.hpp` 移除 `#include "interface.h"` 和 `#include "definition.h"` 两行 (别问, 问就是 feature).

#### 关于某些 C++ 问题

首先, 下发的所有 C++ 代码需要 `-std=c++20`.

在 Policy 类中, 我们用到了 `std::variant`. 简单来说, 对于一个 `std::variant<A,B>`, 其要么存一个 `A` 类要么存一个 `B` 类, 且支持从由 `A` 类构造或者由 `B` 类构造。 你可以参考 [std::variant - cppreference.com](https://en.cppreference.com/w/cpp/utility/variant) 来了解更多。

#### 关于 hack 的部分

我们 **禁止** 在 `generator` 和 `scheduler` 之间用全局变量等手段进行通信. 我们 **禁止** 利用 `oj` 测评端漏洞获取分数, 以及 (在 scheduler 侧) 通过非法手段提前获取在 **当前时刻之后到来的数据**. 如果恶意进攻，计为 $0$ 分，并可能有额外的惩罚。

但是, 我们允许你在 `scheduler` 里面通过特判来识别是否是自己 `generator` 生成的数据.
