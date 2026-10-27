# ESP32-ECGMonitor（中文翻译）

> 本文档为 [README.md](./README.md) 的简体中文翻译。若与英文原版存在不一致，请以英文原版为准。

* 这是一款低成本的连续实时心电图（ECG）监护仪，采用 Espressif 的 ESP32 架构及其 ESP-IDF 框架。代码主体使用 C 语言编写，并托管在 GitHub 仓库中。我们使用的是易于购买、价格低廉的现成元件。

下面是 ECG 系统的一个非常粗略的示意图。注意：图中反映的是 ESP32-DevKit 版本，后来已改用更小型的 ESP32 开发板，即 ESP32S3-supermini 板。

* <img src="./images/ECG-Flow.jpg" alt="ECG 流程图" width="40%">

* 该草图用于说明 ECG 监护系统的工作原理。
    * 电极 **(0)** 贴在受监护的患者身上。
    * 电极导线束连接到 AD8232 **(1)**，这是一个心率前端。
    * AD8232 的信号数据接入 ADS1115 **(2)**，这是一个 16 位模数转换器。
    * 数字化后的数据通过 I2C 串行总线（标记为 **(4)** 和 **(5)** 的导线）被采集，并由 ESP32 DevKit 微控制器 **(3)** 进行处理。
    * 数据和分析结果记录到 MicroSD 转接板 **(6)**。
    * RTC（实时时钟 **(7)**）会提供时间数据并写入 MicroSD 转接板 **(6)**，从而为监测数据的临床技术人员提供带时间戳的 ECG 波形。

* 本项目的主要硬件元件包括：
    - **ESP32 DevKit WROOM-32U**。（注：我最近试用了更小巧的 **ESP32S3 supermini 板**，效果良好）
        * <img src="./images/small-ESP32S3-sm.jpg" alt="ESP32S3" width="30%">
    - **（1 块）ADS1115 转接板。16 位 ADC，4 个 ADC 通道**
        * <img src="./images/ads1115" alt="ADS1115" width="30%">
    - **（完整的 12 电极配置需要 4 块）AD8232 生理监护前端**。
         * <img src="images/ad8232.jpg" alt="AD8232" width="30%">
    - **RTC。用于实时波形更新的时间戳（DS3231）。此外，该板上还有一个 AT24C32，将用作配置文件的存储**
         * <img src="./images/DS3231.png" alt="DS3231" width="30%">
    - **MicroSD 8GB 转接板 —— 记录 ECG 波形（SPI 模式）**
         * <img src="./images/microsd-board.JPG" alt="MicroSD 板" width="30%">

* 本项目的主要软件组件包括：
    * 我们使用 ESP-IDF v6.0.2。这意味着需要使用 I2C_master 驱动。
    * ESP-IDF 软件包内含一个实时操作系统，名为 FreeRTOS。它为数据采集的有序处理以及微控制器承担的各项任务提供了框架。
    * 针对 ECG 监护仪所用各种设备的软件组件 —— 即 ADC 芯片、RTC 板、MicroSD 转接板的驱动程序。
    * 心电数据分析需要处理 QRS 检测，从而得到基本的心率数据。该数据分析组件后续还可以扩展更多功能，用于患者健康状况的监测。

# ECG 信号采集、处理与存储的处理流程
![ECGMonitor](images/ECG-capture-n-proc.png)
* 该图表示 ECG 信号处理的流程。
    * 最左侧是 ADC 采集组件，它将采集到的信号存入数据包（Packet）缓冲区。
    * 当数据包达到最大容量时，就会调用 ECG 处理任务。
    * 该任务包含 QRS 检测，由 Pan-Tompkins 算法完成，得到主要指标：QRS 间期和 R-R 间期。
    * 随后调用 ST 段分析器。
    * 之后可能还会调用一个或多个算法（待定，TBD）。
    * 接下来是数据记录，会生成该数据包波形的时间戳。
    * 数据写入 MicroSD 卡。
    * 如果存在并已配置 LCD 屏幕，也可以同时更新显示新数据。

# 附加文档

# 元件成本估算
参见：COST-ESTIMATES.md

# 个人说明
参见：PERSONAL-NOTES.md

# 一些关于 ECG 监护仪设计的 AI 生成笔记
* [设计笔记](./docs/DESIGN.md) —— 对 ECG 监护仪设计的高层概述，包括硬件与软件架构，以及 ESP32S3 与各外围设备之间的接口。

* [面向创客的电源噪声指南](./docs/Power-Supply-Noise-Guide.md) —— 如何诊断并解决本设计中的 60 Hz 工频干扰与开关电源噪声（电池供电、接地、去耦与 LC 滤波）。
* [如何编写 CMakeLists.txt](./docs/HOW-TO-MAKE-CMAKELISTS.md) —— 一份针对本项目的教程，讲解 ESP-IDF 构建系统、`idf_component_register()` 以及如何新增组件。

# 重新启动 Jupyter Book：

# 1. 激活 Python 虚拟环境
source ../venv/bin/activate

# 2. 启动本地开发预览服务器
jupyter book start .

# 打开该书：http://localhost:3000/
