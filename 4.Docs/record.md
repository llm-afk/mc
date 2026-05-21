- 10.28

  - candump -ta can0监听rk3588的can数据

  - 调试接口：JTAG接口：

    - **5V**
    - **TCK** (Test Clock): 测试时钟
    - **TDO** (Test Data Out): 测试数据输出
    - **TMS** (Test Mode Select): 测试模式选择
    - **TDI** (Test Data In): 测试数据输入
    - **TRST** (Test Reset - 可选): 测试复位
    - **GND**

  - **在CCS（以及很多类似的IDE）中，你不是通过双击某个文件来“打开”工程，而是需要通过“导入 (Import)”的方式将工程添加到你的“工作空间 (Workspace)”中。**

  - 构建后运行的工具

  - 复制文件的命令：

    - ```shell
      ✅ 1. 本地传到远程
      scp /home/user/projects/main.c root@192.168.1.10:/root/
      把本地 /home/user/projects/main.c 复制到远程主机 192.168.1.10 的 /root/ 下。
      
      ✅ 2. 从远程传到本地
      scp root@192.168.1.10:/root/main.c /home/user/projects/
      把远程 /root/main.c 下载到本地 /home/user/projects/ 目录中。
      
      传整个文件夹的话使用scp -r
      ```

- 10.29

  - ssh远程访问的方式：

    ```
    ssh user@192.168.0.1
    ```

  - **ADM32F036C1QN56A**对标的是TI的C2000系列的**TMS320F28035**，CCS的编译配置选项使用的是TMS320F28035的链接文件

  - **TMS320F28035**属于TI的**F2803x**系列，然后该系列下不同的子型号的内核和寄存器架构基本一致，主要差异集中在“存储容量、封装/引脚、外设可用性与数量、工作条件与勘误”。

  - 小端序

  - CLA协处理器内部有8个任务，对应8个触发源可以随意配置。触发源可以是其他外设的信号，也可以是内核的软件信号

    优先级（task1>...>task8）,任务没有执行完不会打断重新触发，低优先级不可以打断高优先级的任务而是等高优先级执行完后执行。

    这里的任务触发可以理解为像stm32那样的外设间配合的事件，在这里，事件可以是定时器中断等等，触发做的事情就不再是ADC触发采集这样的简单的事情，而是一套可以用户自定义的程序，像我们可以把一套完整的电流环的逻辑放在某一个事件的任务中

  - dsp中的pwm是一个独立的模块，名字一般叫做epwm模块，部分epwm有hrpwm的功能拓展（就像stm32上面部分的pwm才有高级的死区互补输出的功能）

  - 一个 **ePWMx** 有两路输出：**ePWMxA** 和 **ePWMxB**。不同的**epwm**的**time-base**模块都是对同一个时钟源分频的

  - 闪存 (Flash) 和静态随机存取存储器 (SARAM) 在物理上是以16位为一个基本单元进行组织的。

    "F28035/34 器件包含 64K x 16 的嵌入式闪存"这句话中的64K表示有64K个**基本存储单元**，然后每个存储单元是16bit的

    每一个地址都对应一个16位的存储空间

  - **\#pragma CODE_SECTION**和**\#pragma DATA_SECTION**将代码和变量放在特定sram块中加速程序的运行和数据的访问

  - CLA协处理器只能访问它自己专属的RAM。主CPU和CLA之间的数据交换需要通过特定的共享RAM块（Message RAM）进行

  - **总线架构:**

    cortex-m内核总线架构，一种**改良的哈佛架构**，因为他们通过总线矩阵仲裁的方式实现了近似fifo的操作，就是你可以有独立的i-bus,d-bus,systerm-bus总线，但是实际的数据获取方式可能是通过总线仲裁的方式获取，具体下面分析两款cortex-m内核的单片机的总线架构进行具体的解释
    
    a.
    
    ![屏幕截图 2025-10-29 154647](.\imgs\屏幕截图 2025-10-29 154647.png)
    这是stm32f103系列的mcu的总线架图，可以看到cpu对外数据访问分为了三条总线，分别是
    
    ### 1. i-bus（Instruction Bus）
    
    - **用途**：专门用于 **从 Flash 或 SRAM 取指令**。
    - **特点**：
      - 与 d-bus（Data Bus）分开，是 Cortex-M3 内核的哈佛特性体现。
      - CPU 执行指令时，通过 i-bus 从 Flash 或 SRAM 获取指令数据。
      - 可以和 d-bus 并行访问数据，总线仲裁器在总线矩阵中负责调度。
    
    ------
    
    ### 2. d-bus（Data Bus）
    
    - **用途**：CPU 执行指令时访问 **数据（RAM/外设/外部总线）**。
    - **特点**：
      - 可以同时访问 SRAM、外设寄存器、外部存储器。
      - 支持读写操作，通常比 i-bus 支持更多类型的访问（比如 DMA）。
    
    ------
    
    ### 3. s-bus（System Bus）
    
    - **用途**：连接 **内核内部系统模块和总线矩阵**，包括：
      - NVIC（中断控制器）
      - SysTick
      - 内部调试/跟踪模块（SW/JTAG, Trace）
    - **特点**：
      - 提供 CPU 核心对系统资源的低延迟访问。
      - 避免 i-bus 或 d-bus 与系统访问冲突，提高实时性。
    
    可以看到ibus是直连到一个flash控制器了，其他的总线（包括DMA）连接到了一个BusMatrix（总线控制器）上来仲裁获取数据，所以可以很直观的看到就是这个架构下CPU访问flash的数据和访问sram的数据是可以并行操作的，但是DMA访问SRAM和CPU访问SRAM需要仲裁。（如果目标地址相同）
    
    ------
    
    b.
    
    ![屏幕截图 2025-10-29 154713](.\imgs\屏幕截图 2025-10-29 154713.png)
    
    | **I-Code bus (I-bus)** | Instruction Code bus | 从 Flash（或 ITCM）取指令        |
    | ---------------------- | -------------------- | -------------------------------- |
    | **D-Code bus (D-bus)** | Data Code bus        | 访问 Flash 中的常量、立即数等    |
    | **System bus (S-bus)** | System bus           | 访问 SRAM、外设、DMA、总线矩阵等 |

​	所以可以看到cpu访问flash中的指令和常量等和cpu访问SRAM、外设、DMA、总线矩阵等是可以真正并行的。其他的数据访问需要经过总线矩阵路由到相关的地址

c.

![屏幕截图 2025-10-29 162521](.\imgs\屏幕截图 2025-10-29 162521.png)

dsp的总线架构相比cortex-m的总线架构，对于一些特定的数据访问使用了专用的总线，让cpu直接访问而不是通过busMartix来桥接带来可能的不确定性

这里可以看到cla和cpu的总线是完全独立分隔的，这意味着他们永远不会冲突。类似的还有很多的MemoryBus中其实还有很多细分的总线也是独立的这里没有显示出来。可以认为所有的块中标明(0-wait)的都是有独立的点对点总线的，不会有仲裁

总之：dsp的架构优势就是举个特定的关键的例子，一个MAC的指令，触发的多个操作，为了达到最高数据吞吐，DSP通常会**同时从两个不同的数据RAM块**

dsp有x/y双数据总线，也就是可以一个周期访问两个数据，但是arm只有一个数据总线所以一个时钟周期只能访问到一个数据。

所以dsp的MAC指令是高度优化过的，一条流水线上完成

1. **IF (Instruction Fetch)**: 取指令
2. **ID (Instruction Decode)**: 解码指令
3. **EX (Execute)**: 执行运算（包括并行读取操作数）
4. **MEM (Memory Access)**: （这里DSP的EX阶段已经做了，我们简化为其他内存操作）
5. **WB (Write Back)**: 写回结果

指令执行在单周期完成所有的步骤，但是延迟了5个周期

  - 

    - 查看实际的工程中选择的芯片型号和仿真器类型![屏幕截图 2025-10-29 184237](.\imgs\屏幕截图 2025-10-29 184237.png)
    - 新建的工程的原始工程格式<img src=".\imgs\屏幕截图 2025-10-29 184620.png" alt="屏幕截图 2025-10-29 184620" style="zoom:200%;" />                                                      
    - 官方给了两种工程的配置模式**Debug**和**Release**（测试模式和发布模式）其中会设置不同的编译优化等级。可以右键工程里的build Configueration里面的set active里面设置当前选择的配置模式。这是官方给到的一种标准化的开发方式。然后我们可以在右键工程后的properties后的页面中设置不同的工程配置选项：例如编译器版本、目标芯片、输出格式、连接命令文件等
    - 狗屎的，关键部分文件仅提供obj文件，没有C源码

      | **阶段**         | **TI DSP (CCS)** | **STM32 (Keil/GCC)** | **文件角色**                               |
      | ---------------- | ---------------- | -------------------- | ------------------------------------------ |
      | **编译输出**     | .obj             | .o / .obj            | 单个源文件的机器码（半成品）               |
      | **链接输出**     | **.out**         | **.elf / .axf**      | **可执行、可调试的最终产物，包含所有信息** |
      | **格式转换输出** | .hex / .bin      | .hex / .bin          |                                            |
    - ccs选择或不选择某文件参与工程的编译：右键某工程，然后选择**Exclude From Project**,然后该选项处于打勾状态表示当前这个文件没有包含在工程中
    - **增量式编译**(**锤子图标**)和全量编译的图标，还有**快速选配置模式**的选项                                         ![屏幕截图 2025-10-29 194216](.\imgs\屏幕截图 2025-10-29 194216.png)![屏幕截图 2025-10-29 194238](.\imgs\屏幕截图 2025-10-29 194238.png)
    - 编译器版本选择：建议选择最新的22版本，然后为了方便可以一直只用一个debug的配置选项![屏幕截图 2025-10-29 201328](.\imgs\屏幕截图 2025-10-29 201328.png)
    - 编译器相关的配置介绍：![屏幕截图 2025-10-29 202527](.\imgs\屏幕截图 2025-10-29 202527.png)



| 导航栏选项 (Navigation Pane Option)    | 核心功能 (Core Function)                                     | 关键设置与作用 (Key Settings & Effects)                      | 好比是... (Analogy)                                          |
| -------------------------------------- | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| **Processor Options**<br>(处理器选项)  | **定义硬件目标**<br>告诉编译器代码将在什么样的CPU上运行。    | **--silicon_version**: 芯片具体型号，影响指令集。<br>**--float_support**: 是否有硬件FPU，决定浮点运算效率。<br>**--cla_support / --tmu_support**: 是否有CLA/TMU协处理器。 | 给翻译官一份**作者的详细简历**，让他知道作者的专业领域和习惯用语。 |
| **Optimization**<br>(优化)             | **控制代码质量**<br>在性能、大小和可调试性之间做权衡。       | **-O / --opt_level**: 优化等级 (0-4)，决定优化的深度。<br>**-mf / --opt_for_speed**: 优化倾向，优先考虑速度还是体积。<br>**--fp_mode**: 浮点运算的精度模式 (严格或宽松)。 | 指示翻译官这份稿件的**翻译风格要求**：是要求信达雅的文学翻译，还是快速直白的机器翻译。 |
| **Include Options**<br>(包含选项)      | **指定头文件路径**<br>告诉编译器去哪里查找 #include 的 .h 文件。 | **-I / --include_path**: 添加头文件所在的文件夹路径。这是解决“找不到文件”错误的关键。 | 给翻译官**一套专业领域的参考词典**，告诉他遇到专业术语时去哪里查。 |
| **Performance Advisor**<br>(性能顾问)  | **静态代码分析**<br>检查代码并提供性能改进建议。             | 启用/关闭此功能。它会分析代码并给出潜在的性能瓶颈警告。      | 一位**资深编辑**，在翻译官交稿前，帮他审阅稿件，并提出一些改进建议。 |
| **Predefined Symbols**<br>(预定义符号) | **全局宏定义**<br>在编译开始前就定义一些宏，用于条件编译等。 | **-d / --define**: 添加MACRO=VALUE。等同于在所有文件的开头写#define。 | 在翻译开始前，给翻译官一张**“特别注意事项”清单**，比如“本文档中的‘公司’一词特指XX公司”。 |
| **Advanced Options**<br>(高级选项)     | **精细化控制**<br>提供对语言标准、诊断信息等高级特性的控制。 | **--c_std**: 强制使用的C语言标准 (如c99, c11)。<br>**诊断选项 (Diagnostics)**: 压制/提升特定警告的等级。 | 一份**高级排版和语法指南**，让翻译官可以控制最终译文的每一个细枝末节。 |

![屏幕截图 2025-10-29 203343](.\imgs\屏幕截图 2025-10-29 203343.png)

| 等级    | 名称         | 优化范围              | 最佳用途     |
| ------- | ------------ | --------------------- | ------------ |
| **off** | 关闭优化     | 无                    | 调试         |
| **0**   | 寄存器优化   | 表达式                | 调试         |
| **1**   | 局部优化     | 基本代码块            | -            |
| **2**   | 全局优化     | 单个文件 (.c)         | **标准发布** |
| **3**   | 过程间优化   | 跨函数 (单个文件内)   | 高性能发布   |
| **4**   | 整个程序优化 | **整个项目 (跨文件)** | 最终极限优化 |

![屏幕截图 2025-10-29 203604](.\imgs\屏幕截图 2025-10-29 203604.png)

前面的0-选择优化的范围，后面的滑块选择优化的程序，是速度优化还是体积优化

![屏幕截图 2025-10-29 204013](.\imgs\屏幕截图 2025-10-29 204013.png)

性能分析等

好的编译器的配置上配置了优化、包含路径、性能优化建议、预定义等内容

- 10.30

  - TI的DSP系列分类及命名规则:

    TI的DSP产品以其强大的信号处理能力而闻名，主要分为三大系列：C2000™、C5000™和C6000™，此外还有集成了ARM处理器的DaVinci™和OMAP™系列。所有DSP产品通常以“TMS320”作为前缀。

    #### **DSP产品线概览**

    | 系列         | 定位         | 主要特点                             | 典型应用                                   |
    | ------------ | ------------ | ------------------------------------ | ------------------------------------------ |
    | **C2000™**   | 实时控制     | 强大的PWM控制、高精度ADC、快速响应   | 电机控制、数字电源、太阳能逆变器、工业驱动 |
    | **C5000™**   | 超低功耗     | 极低的待机和工作功耗、高效的定点运算 | 便携式音频设备、医疗设备、生物识别         |
    | **C6000™**   | 高性能       | 强大的浮点和定点运算能力、多核架构   | 通信基础设施、高级成像、雷达、专业音频     |
    | **DaVinci™** | 数字媒体处理 | 集成DSP和ARM核、视频和音频加速器     | 数字视频监控、视频会议、媒体播放器         |

    - #### **1. C2000™ 实时控制器**

      - ### **TI C2000™ 实时控制器产品线概览**

        | 系列                            | 产品型号              | 内核组成                                                  | 定位                                                         |
        | ------------------------------- | --------------------- | --------------------------------------------------------- | ------------------------------------------------------------ |
        | **Piccolo™** <br> (入门-中端)   | TMS320F2802x          | 1x C28x 定点 CPU                                          | **成本敏感型基础控制**：适用于对成本要求极致，算力要求不高的应用。 |
        |                                 | TMS320F2803x          | 1x C28x CPU + 浮点单元(FPU) + 控制律加速器(CLA)           | **入门级浮点与协处理**：以较低成本提供浮点运算和CLA并行处理能力。 |
        |                                 | TMS320F2805x / F2806x | 1x C28x CPU + FPU + CLA + VCU (通信协处理器)              | **中端市场全能主力**：性能均衡，功能全面，特别是F2806x集成InstaSPIN™电机技术。 |
        | **新一代** <br> (高性价比)      | TMS320F28002x         | 1x C28x CPU + FPU + CLA                                   | **新一代入门级标杆**：F2802x/3x的升级首选，以极具竞争力的成本提供了浮点运算等高级功能。 |
        |                                 | TMS320F28004x         | 1x C28x CPU + FPU + CLA                                   | **新一代中端市场主力**：F2806x的升级替代品，拥有更快的CPU和ADC，性价比极高。 |
        | **Delfino™** <br> (高性能-旗舰) | TMS320F2833x          | 1x C28x CPU + FPU                                         | **经典高性能浮点控制**：久经市场考验的旗舰型号，算力强，外设丰富。 |
        |                                 | TMS320F2837xS (单核)  | 1x C28x CPU + FPU + 1x CLA                                | **单核性能旗舰**：代表C2000单核处理能力的顶峰，拥有更快的CPU和更先进外设。 |
        |                                 | TMS320F2837xD (双核)  | **2x** C28x CPU + FPU + **2x** CLA                        | **双核实时控制巅峰**：提供强大并行处理能力，可将控制、通信、诊断等任务完全分离。 |
        |                                 | TMS320F2838x (多核)   | **2x** C28x CPU + FPU + **2x** CLA + **1x ARM Cortex-M4** | **控制与通信融合的顶级方案**：在双核控制基础上增加ARM M4核处理复杂通信协议（如EtherCAT）。 |

      #### **2. C5000™ 超低功耗DSP**

      C5000系列专为需要高效信号处理同时功耗预算极为严格的便携式应用而设计。[[3](https://www.google.com/url?sa=E&q=https%3A%2F%2Fvertexaisearch.cloud.google.com%2Fgrounding-api-redirect%2FAUZIYQE3kgEbtiGyovBVITrJlqxzFpPtBtA3BnzQta_LtJDobzHR4VroAz_8fSaGXEnGsPyVgtjFBuvFQXHxWtC1s4oCO3IHipTjo5p6H06-DPkrGOgI6fv9HCfgBXs_xGkLUGUYOHUQqw0JjNccn5sPIU7sH9_fo8-EtY9d5cNUm856R_l2qm_6Qoc7MyD1g2B1kf8VwI12zOZv6EYKsgIPHbhsRXVFpYrn_cVI0aUQoRZdYMXvqxd7OEnhs6ZOgiR_Pg%3D%3D)]

      - **产品系列:** 主要包括TMS320C55x系列，如C5515、C5535和C5545。这些处理器在提供足够性能的同时，拥有业界领先的低功耗表现。
      - **命名方式解析 (以TMS320C5515为例):****TMS320:** TI DSP的家族代号。**C:** 通常表示CMOS工艺。**55:** C5000系列的代号，特指C55x内核。**15:** 具体的型号。
      - **产品定位:** 适用于电池供电的设备，如便携式医疗设备、录音笔、可穿戴设备和某些物联网终端。[[3](https://www.google.com/url?sa=E&q=https%3A%2F%2Fvertexaisearch.cloud.google.com%2Fgrounding-api-redirect%2FAUZIYQE3kgEbtiGyovBVITrJlqxzFpPtBtA3BnzQta_LtJDobzHR4VroAz_8fSaGXEnGsPyVgtjFBuvFQXHxWtC1s4oCO3IHipTjo5p6H06-DPkrGOgI6fv9HCfgBXs_xGkLUGUYOHUQqw0JjNccn5sPIU7sH9_fo8-EtY9d5cNUm856R_l2qm_6Qoc7MyD1g2B1kf8VwI12zOZv6EYKsgIPHbhsRXVFpYrn_cVI0aUQoRZdYMXvqxd7OEnhs6ZOgiR_Pg%3D%3D)]

      #### **3. C6000™ 高性能DSP**

      C6000系列是TI性能最强的DSP平台，提供定点和浮点两种类型，并包含多核处理器。

      - **产品系列:****C67x/C674x:** 浮点DSP系列，适用于需要高精度计算的应用。**C66x:** 高性能多核浮点DSP系列，如C6678（八核），是TI KeyStone架构的核心。**C64x+:** 高性能定点DSP系列。
      - **命名方式解析 (以TMS320C6678为例):****TMS320:** TI DSP的家族代号。**C:** 通常表示CMOS工艺。**66:** C6000系列中的C66x内核。**78:** 具体的型号，最后一个数字‘8’通常表示内核数量。
      - **产品定位:** 广泛应用于需要强大实时信号处理能力的领域，如4G/5G通信基站、专业视频处理、高端成像（如医疗成像）以及国防应用。

      ------

  - .obj文件和.lib文件：

    - .obj文件可以理解成一个闭源的编译好的源文件，向外暴露了一些函数和变量，外部如果提前知道的话，就可以通过提前声明然后使用。
    - .lib文件可以理解成多个.obj文件的整合。很多官方的库都是以这个形式给出的。间接的参与我们工程的编译

- 10.31

  - jtag接口的vref是用来确定通信的电平标准的，不具备供电能力，主板需要额外供电

  - ADM32F036C1的TTL电平标准是5V

  - 双击设置断点

  - debug as进入debug

  - dsp中的cmd文件用于链接时的内存分配。

  - map文件是dsp、mcu等链接后都有的产物，用于描述所有的存储的实际地址（dsp和mcu的形式略有不同）

  - debug进去去然后排查的问题

    - 首先是上电后使用ccs的调试器的verify验证调试器功能：打印输出内容说是调试器通过TDI发送数据但是板子没有通过TDO给应答数据所以失败，于是开始排查jtag接线问题
    - 芯片的IO电平是5V，所以理论上调试器的VREF也要接5V，但是调试器的VREF接板子上的5V，会把板子上的5V拉低到3V且主控芯片发热
    - 查看进芯调试器XDS110，说是宽电压工作范围：支持1V~5V
    - 尝试把jtag的3.3V直接接在VREF上，这下SDI和CLK和SDO都没有输出了
    - 调试器选择XDS110
    - 排查了半天，原因是这个烧录器上的JTAG座子的线序和标准的JTAG线序不一样导致实际的引脚没有正确连接

  - 有两个工程选项：DEBUG 和 RELEASE。都可以分别设置他们的参数，例如编译器版本，优化范围和等级，还有头文件包含路径，链接文件类型等

  - 报**error716**错误：工程的配置下的仿真器类型没有选择正确

  - 报**error1135**错误：jtag通信没有成功：板子没有上电、芯片坏了、jtag线没有接对等

  - **关于cmd文件你所需要知道的一切：**

    - .cmd文件是用于在连接器阶段将所有的obj等过程文件中的变量和函数等规划他们的flash存储位置（也有ram的，但是我们主要关心的是flash，就像我们在keil中写bootloader和app的时候，我们会定义他们不同的flash起始地址和实际的flash占用的空间，意味这两个程序的ram其实是共用的一块起始的地址，除非我们希望一些特殊的变量存放在ram中的某一个区域，就像bootloader和app通信的一个变量就可以放在ram的一个区域，但是一般我们都是直接根据地址确定好一个地址然后上方约定好直接访问这个地址来做的）

    - **为什么有的工程有两个.cmd文件，一个cmd文件完成所有的配置不是很完美吗？**

      在ti的设计中，相同型号的芯片的寄存器描述是相同的，所以都抽象出来了一个统一的可以复用的.cmd文件，就是那个phripheral结尾的.cmd文件。然后另外一个就是描述程序的所有的程序和变量和函数实际的位置的cmd的文件。

    - CCS怎么知道我这个工程包含了哪些cmd文件？

      好的，两个需要关注的点：

      - 第一点：项目中**直接包含**了哪些.cmd文件，就默认他们都参与链接，不需要的可以删掉或者右键然后**Excliude From Peojext**
      - 第二点：用于纠正一些误解：键入工程的配置后，里面的general里面有一个选择cmd文件的下拉框，这玩意不是选择这些cmd文件参与编译，而是将这些cmd文件**添加进工程**。仅此而已。

- 11.3

  - DEBUG相关：

    - 下载都是通过调试的方式将程序放进去的，根据不同的cmd文件

  - 源工程是基于“**寄存器位域头文件 + 全局寄存器对象 + 链接脚本映射**”的开发方式，我们可以看到有一个专门的“**DSP28035Peripheral.cmd**”这个链接命令文件将“**全局寄存器对象**”放到相关寄存器的实际的物理地址上

  - cmd文件内部分为两个部分：memory和section

    - **MEMORY（定义物理地址“哪里”）**

      给每个外设寄存器窗口起名并标注起始地址与长度，PAGE=1 表示数据/外设空间。

    - **SECTIONS（把“段”放到上述物理地址“哪里”）**

      用 段名 : > 区域名, PAGE = n 把编译出的段映射进对应的 MEMORY 区域。

    - 段名从哪来（C 文件里的 #pragma）

  - 在工程中用C写**\#pragma DATA_SECTION(变量, "段名")**

  - dsp优化技巧：

    - 使用三元运算符
    - 浮点数计算使用f后缀的明确定义为单精度浮点数，否则会执行双精度的浮点运算，造成很多额外的开销

  - cputimer用于代码的性能分析，类似于arm的dwt

  - 找到了dsp中的软重启的部分函数，原理是手动的触发开门狗超时达到复位的目的，可以用于app跳bootloader

- 11.4

  - ```c
    void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* CanHandle)
    {
        /* Get RX message */
        if (HAL_CAN_GetRxMessage(CanHandle, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK)
        {
            /* Reception Error */
            Error_Handler();
        }
    
        uint8_t id = (RxHeader.StdId >> 7); // 4Bits ID & 7Bits Msg
        uint8_t cmd = RxHeader.StdId & 0x7F; // 4Bits ID & 7Bits Msg
        if (id == 0 || id == boardConfig.canNodeId)
        {
            OnCanCmd(cmd, RxData, RxHeader.DLC);
        }
    }
    ```

    这是稚辉君开源的代码中的关于can接口通信相关的部分，可以看到他这里没有做滤波器，而是直接软件做的过滤，然后11位的canid,高4位用作nodeid，低7位用作命令字。

  - nodeid中的node是节点的意思

  - 看了稚辉君的关于can通信的部分和canopen通信的部分，他们对于11位的canid的处理虽然形式上不一样，但是目的都是差不多的，就是在这11位的canid里面区分这一个帧的**nodeid**和**命令字**，只是分配的大小和方式不同

  - ![a20983276ef5671a10724d2f2f8dc07e](.\imgs\a20983276ef5671a10724d2f2f8dc07e.jpg)

    ![576420aa551b268e518e96e626c0cf8d](.\imgs\576420aa551b268e518e96e626c0cf8d.png)

    ![566bf5ec4ed3faa9821d0ecda83ae068](.\imgs\566bf5ec4ed3faa9821d0ecda83ae068.png)

- 11.5

  - .obj文件是一种预编译库
  
  - 关键的算法部分以.obj预编译库的形式提供，然后给一个调用的头文件接口声明有哪些api函数可用
  
  - 大量的全局变量
  
  - 函数调用内部操作的全是全局变量，几乎没用参数和返回值
  
  - 500HZ速度环
  
  - \#define T_PRD  (Uint16)(((unsigned long)DSP_CLOCK60 * 5000) / PWM_FREQUENCY)这里有问题：用的是60M的时钟频率，修改为100M
  
  - 修改PWM_FREQUENCY参数为200将实际的pwm频率修改成20KHZ
  
  - ![屏幕截图 2025-11-05 161144](.\imgs\屏幕截图 2025-11-05 161144.png)
  
    - | 编译选项                | 推荐用于 "Debug" 配置 | 推荐用于 "Release" 配置 | 理由                                                         |
      | ----------------------- | --------------------- | ----------------------- | ------------------------------------------------------------ |
      | **Optimization level**  | **off** (或 0)        | **3** 或 **4**          | **Debug**：保证代码可调试。<br>**Release**：发挥芯片最大性能。 |
      | **Speed vs. size**      | 3 (或任意)            | **5 (speed)**           | **Debug**：此项无效。<br>**Release**：实时控制，速度优先。   |
      | **Floating Point mode** | strict                | strict                  | 保证算法的数学精度，除非有特殊需求。                         |
  
  - 启用 C99：工程属性 → Build → C2000 Compiler → Language Options → 勾选/填入 --c99，再编译。
  
  - 汇编写的启动代码中干的事：关开门狗、清空RAM等
  
  - DSP上电流程：
  
    - 1.上电/复位后先执行芯片内部 BootROM：上电自检、决定启动源（GPIO/OTP 配置），若为“Boot to Flash”，跳转到链接脚本 F28036.cmd 中的 BEGIN 段入口。
  
      ```c
      SECTIONS
      {
         ...
         codestart           : > BEGIN       PAGE = 0
         ramfuncs            : LOAD = FLASHA,
                               RUN = RAMPRG,
                               ...
      }
      ```
  
    - 2.进入项目的汇编入口 code_start，转入自定义 early init
  
      ```C
      code_start:
          LB main_init       ;Branch User Code:One instruction only
      ```
  
    - 3.早期初始化：切到 28x 模式、先关看门狗、清零片上 RAM、准备 C 运行时
  
      ```c
      main_init:
          SETC 	OBJMODE        		;Set OBJMODE for 28x object code
          EALLOW              		;Enable EALLOW protected register access
          MOVZ 	DP, #7029h>>6  		;Set data page for WDCR register
          MOV 	@7029h, #0068h  	;Set WDDIS bit in WDCR to disable WD
          EDIS                		;Disable EALLOW protected register access
      
      	MOV		ACC,	#00H	
      	MOVL 	XAR5,	#0000H		;Clear M0
      	MOVL 	XAR4,	#(400H-1)
      	RPT		@AR4
      	|| MOV	*XAR5++,	ACC
      
      	MOVL 	XAR5,	#0400H		;Clear M1
      	MOVL 	XAR4,	#(400H-1)
      	RPT		@AR4
      	|| MOV	*XAR5++,	ACC
      
      	MOVL 	XAR5,	#8000H		;Clear L0, L2, L3
      	MOVL 	XAR4,	#(1000H-1)
      	RPT		@AR4
      	|| MOV	*XAR5++,	ACC
      	
      	MOVL 	XAR5,	#9000H		;Clear L1
      	MOVL 	XAR4,	#(3000H-1)
      	RPT		@AR4
      	|| MOV	*XAR5++,	ACC
       
      	LB _c_int00					;跳转到C运行时入口
      ```
  
    - 4.进入 TI 运行库入口 _c_int00（RTS 完成）
  
      初始化堆栈、复制 .cinit 到数据段、清零 .bss、调用全局构造器（C++）、然后调用 main()。
  
    - 5.跳转用户定义的mian函数
  
  - 一般来说所有的外设的配置寄存器写入的时候都需要在EALLOW;和EDIS;之间完成
  
  - 所有的头文件都要将头文件路径添加到工程的虚拟include中
  
  - inline\extern inline\static inline
  
  - 用extern显式的声明的目的是为了表明这个函数是引用的其他文件的函数
  
- 11.6

  - 捕捉到一个问题：就是之前还是用提供的cmd文件按还没有改的时候，debug进去的时候程序是可以正常跑起来的，运行没问题，但是掉电重新上电后的话就有问题了，程序不是正常运行的。可是明明我的cmd文件里面是将程序段放在flash里面的啊：

    排查了之后：将systemInit中的初始化PLL和FLASH控制器交换一下顺序即可

  - DSP的引脚分布规律：ADC的输出固定为那几组ADC专用，PWM等外设的信号想要输出和交互需要走GPIO，固定的机几组GPIO可以选择

  - 预驱模块自带0.2uS硬件死区，可防止H桥直通。

  - 首先 DSP 的所有 RAM/Flash 地址都是统一编址的（唯一地址）。

  - 如果是要让 CPU/CLA 执行的代码（指令），就把这片空间放到 PAGE 0。

  - 如果是要让 CPU/CLA 读写的数据，就把这片区域放到 PAGE 1

  - PAGE 只是告诉链接器：这个段是“程序”还是“数据”，从而让 CPU 用指令总线或数据总线访问它。

  - ```C
    MEMORY
    {
    PAGE 0:    /* Program Memory */
       RAMPRG      : origin = 0x008000, length = 0x000A80     /* on-chip RAM block L0, for RamFuncs */
       OTP         : origin = 0x3D7800, length = 0x000400     /* on-chip OTP */
       FLASHA      : origin = 0x3E8000, length = 0x00FF80     /* on-chip FLASH */
       CSM_RSVD    : origin = 0x3F7F80, length = 0x000076     /* Part of FLASHA.  Program with all 0x0000 when CSM is in use. */
       BEGIN       : origin = 0x3F7FF6, length = 0x000002     /* Part of FLASHA.  Used for "boot to Flash" bootloader mode. */
       CSM_PWL     : origin = 0x3F7FF8, length = 0x000008     /* Part of FLASHA.  CSM password locations in FLASHA */
    
       IQTABLES    : origin = 0x3FE000, length = 0x000B50     /* IQ Math Tables in Boot ROM */
       IQTABLES2   : origin = 0x3FEB50, length = 0x00008C     /* IQ Math Tables in Boot ROM */
       IQTABLES3   : origin = 0x3FEBDC, length = 0x0000AA	  /* IQ Math Tables in Boot ROM */
    
       ROM         : origin = 0x3FF27C, length = 0x000D44     /* Boot ROM */
       RESET       : origin = 0x3FFFC0, length = 0x000002     /* part of boot ROM  */
       VECTORS     : origin = 0x3FFFC2, length = 0x00003E     /* part of boot ROM  */
    
    PAGE 1 :   /* Data Memory */
               /* Memory (RAM/FLASH/OTP) blocks can be moved to PAGE0 for program allocation */
               /* Registers remain on PAGE1                                                  */
    
       RAMM0       : origin = 0x000100, length = 0x000300     /* on-chip RAM block M0 */
       RAMM1       : origin = 0x000400, length = 0x000400     /* on-chip RAM block M1 */
       RAML0       : origin = 0x008A80, length = 0x001580     /* on-chip RAM block L0 */
       RAMH0	   : origin = 0x00A000, length = 0x002000     /* on-chip RAM block H0 */
    /*   RAML1	   : origin = 0x009000, length = 0x001000*/	  /* on-chip RAM block L1 */
    }
    
    ```

    在 MEMORY 配置里，将 L0 RAM 的一部分标记为 RAMPRG，放在 PAGE0，用于存放需要在 RAM 中执行的程序段（如 ramfuncs），供 CPU 通过指令总线访问。

    同时，我可以自己衡量和设置分配的大小，只要符合我的需求

    这里分配的长度单位是字节（8位），推荐按照2字节的倍数的起始地址和长度分配空间，这样访问的时候不会有而外的性能损失

  - Memory Browser（内存浏览器）的使用：可以看到内存中变量的名称和实际的地址和数据等信息。

  - 自己指定cla的程序段的地址和大小，从L0RAM中分配

  - M0、M1、L0、L1的RAM分类：M系列的用于消息交互，L系列的用于变量访存和高速程序运行

  - 1.DSP上电/复位，CPU跳转执行复位向量中的指令（地址0x3FFFC0）；

    2.运行boot loader，选择启动模式；

    3.跳转到默认入口地址（对于常用的从flash启动，地址是0x33FFF6）；

    3点5.（可选）关看门狗；

    4.跳转到_c_int00程序；

    5.执行_c_int00程序；

    6.执行main函数。

  - 本来有个只有debug里面才能运行程序，发现是不处于debug时要把jlink拔掉，应该是boot的引脚和jlink冲突了然后导致程序选择错误

- 11.7

  - ADM32F036C1QN56A的CCS模板工程创建

    - 创建新的工程，选择芯片为035，仿真器为110，编译器为v22，cmd文件为空后续手动添加
    - 添加好工程的文件空间cmd文件夹+usr文件夹(bsp+driver+application+middleware)
    - 添加好cmd文件和一些必要的启动文件和库文件和一些最基础的驱动
    - 添加工程的虚拟inckude路径
      - ${PROJECT_ROOT}/User/BSP/HAL
        ${PROJECT_ROOT}/User/Middleware
        ${PROJECT_ROOT}/User/Driver
        ${PROJECT_ROOT}/User/Application
    - 然后后续基于这个模板要使用的话，直接再ccs的文件管理界面直接复制粘贴一份，改个名字就可以用了

  - 单个.c文件编译成.obj(.o)文件->大量obj文件压缩成.a(.lib)文件

  - 向量表：

    - 首先要知道的就是bootrom这个东西里面存了哪些东西：
      - 官方的bootloader
      - 中断向量表
      - 一些数学表
      - 一些数据
    - dsp上电后第一件事就是固定从0x3FFFC0地址（bootrom中固化）下取指令执行，这个地址里面存储着一条跳转指令，如果翻译成汇编语言的话就是“LB #0xXXXXXX”，这是一条无条件跳转指令，跳转去执行bootloader程序
    - bootloader做了一些系统的初始化然后最终根据用户指定的启动模式去跳转到不同的地址（就像arm里面选择从arm启动还是mainflash里启动）

  - 

    

