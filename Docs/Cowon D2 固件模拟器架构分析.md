# Cowon D2 固件模拟器架构分析

这段代码是一个**完整的 Cowon D2 播放器固件模拟器 + NAND 嗅探器**，运行在 Windows 上，用 x86 主机模拟 TCC780x 系列 ARM 处理器执行 D2 固件。核心目标不是“运行固件”，而是**通过模拟执行来嗅探固件对 NAND、I2C、LCD 等外设的访问，从而 DUMP 出原始数据**。

下面按架构层次拆解。

---

## 一、整体架构

```text
┌─────────────────────────────────────────────────────────┐
│  Windows 宿主层                                          │
│  RunCode() 主循环 / CrashVectored / SendMessage UI       │
├─────────────────────────────────────────────────────────┤
│  CPU 模拟层 (CORE cpu)                                   │
│  ARM/Thumb 指令执行 / 异常 / 模式切换 / 中断             │
├─────────────────────────────────────────────────────────┤
│  地址空间层 (read_write)                                 │
│  DRAM / IRAM / CACHE / EEPROM / MMIO 寄存器              │
├─────────────────────────────────────────────────────────┤
│  MMIO 寄存器映射层 (get_register / iptr / optr / mptr)   │
│  动态寄存器分配 + 访问计数 + 写数据记录                   │
├─────────────────────────────────────────────────────────┤
│  外设模拟层                                              │
│  NFC(NAND) / I2C / LCD / GPIO / Timer / Interrupt        │
├─────────────────────────────────────────────────────────┤
│  NAND 镜像层                                             │
│  read_value / write_value / wrbin / nand_perfect.bin     │
├─────────────────────────────────────────────────────────┤
│  日志与可视化层                                          │
│  ata.txt / lcd.txt / fb_*.bin / reg.txt / accessed2.txt  │
└─────────────────────────────────────────────────────────┘
```

它和普通模拟器最大的区别是：**模拟的主要目的不是“跑通程序”，而是“记录程序对外设做了什么”**，因此每个模块都带大量日志、dump、快照。

---

## 二、内存模型

### 1. 存储区划分

```c
ULONG iram[IRAM_SIZE/4];   // 内部 RAM
ULONG dram[DRAM_SIZE/4];   // 主 DRAM，固件加载到这里
ULONG cach[CACH_SIZE/4];   // 缓存
ULONG eepr[EEPR_SIZE/4];   // EEPROM
```

`read_write()` 根据地址范围路由到不同存储：

| 地址范围 | 存储 |
|---|---|
| `DRAM_ADDR ~ +DRAM_SIZE` | `dram` |
| `IRAM_ADDR ~ +IRAM_SIZE` | `iram` |
| `CACH_ADDR ~ +CACH_SIZE` | `cach` |
| `EEPR_ADDR ~ +EEPR_SIZE` | `eepr` |
| 其他（如 `0xF0053000`） | MMIO 寄存器 |

DRAM 地址做了 `(des & 0x1ffffff) >> 2` 的索引转换，说明 D2 的 DRAM 基址在 `0x20000000` 附近，物理映射到数组。

### 2. 固件加载

```c
char *executable = "D:\\d2.bin";
fread(dram, sz, 1, fil);
cpu.rg[15] = DRAM_ADDR;   // 入口
```

固件被原样加载到 `dram`，PC 从 `DRAM_ADDR` 开始执行。也就是说，这是一个**裸机级模拟**，固件直接跑，不走 OS。

---

## 三、CPU 模拟层

### 1. CORE 结构

```c
CORE cpu;
cpu.rg[16]    // 16 个通用寄存器
cpu.reg[16]   // 指向 rg[] 的指针数组，可动态指向不同 bank
cpu.cpsr      // 指向 rg[16]
cpu.spsr      // 指向各模式下 SPSR
cpu.old_mode
cpu.int_en, int_pending, int_stat, irq_return
```

### 2. 双指令集

主循环 `RunCode()` 里根据 `cpu.cpsr->T` 区分：

- **Thumb 模式**：`GETUS(pc)` 取 16 位，按 `opcode >> 11` 分派到上一文件里那些函数。
- **ARM 模式**：`GETUL(pc)` 取 32 位，按 `(opcode >> 24) & 15` 分派到：
  - `opcode()`
  - `single_transfer()`
  - `multip_transfer()`
  - `branch()`

ARM 模式下还特殊处理了一条指令：

```c
if((cpu.opcode & 0x0fffffff) == 0xe17ff7a) cpu.cpsr->Z = 1;
// test, clean, invalidate DCache
```

这是**把缓存维护指令直接当成成功返回**，因为模拟器没有真实 cache。

### 3. 模式切换

`switch_cpu_mode()` 实现了 ARM 七种模式：

| 模式 | 值 | 寄存器 bank |
|---|---|---|
| User/System | 0x1F/0x10 | 无 bank |
| FIQ | 0x11 | R8~R14 + SPSR |
| IRQ | 0x12 | R13,R14 + SPSR |
| Supervisor | 0x13 | R13,R14 + SPSR |
| Abort | 0x17 | R13,R14 + SPSR |
| Undefined | 0x1B | R13,R14 + SPSR |

通过 `cpu.reg[ii] = &cpu.rg[...]` 动态切换寄存器指针，这是很经典的 ARM 模拟技巧。

### 4. 异常与中断

- `switch_to_interrupt()`：保存 CPSR 到 SPSR，切模式，设置 LR，PC 跳到向量表，清 T 位，置 I 位。
- `switch_from_interrupt()`：从 SPSR 恢复 CPSR，切回模式。
- `trigger_interrupts()`：
  - 检测 `tim1/tim2` 到 0 触发 TIMER0 中断。
  - 检测 `irq_return` + `last_instr_moved_pc()` 判断“刚从中断返回”。
  - 根据 `int_pending` 分发 TIMER0 / TIMER2 / MBOX / DMA 中断。
  - Thumb SWI (`0xdf00`) 触发 0x08 向量。

这里有个**模拟器专用 hack**：

```c
case 0x04000000: // DMA interrupt
  c->int_pending = 0x200;   // 强制再触发一次 0x200
```

因为固件对 DMA 中断的响应方式与模拟器不完全一致，这里人为补一个中断。

---

## 四、MMIO 寄存器映射层（核心创新）

这是整个模拟器**最精巧的设计**。

### 1. 问题背景

TCC780x 有大量 MMIO 寄存器（NFC、I2C、GPIO、LCD、Timer…）。如果每个都写死一个变量，代码会爆炸，也不方便动态发现“固件访问了哪些寄存器”。

### 2. 解决方案：动态寄存器池

```c
ULONG iptr[512]; // 目标寄存器地址
ULONG optr[512]; // 映射后的存储值
ULONG mptr[512]; // 排序后的索引（用于二分查找）
ULONG aptr[512]; // 访问次数
ULONG bptr[512]; // 写访问次数
ULONG idat[512][256]; // 写入的数据序列
```

`get_register(des)` 的逻辑：

1. 用二分查找在 `mptr` 里找 `des`。
2. 找到就返回 `&optr[i]`，并 `aptr[i]++`。
3. 没找到就插入新条目，分配 `optr[i]`，按需给默认值。
4. 记录访问次数和写入数据。

这样做的效果：

- **不需要提前定义所有寄存器**，固件碰到什么就动态注册什么。
- **自动统计每个寄存器的访问频率和写入序列**，方便逆向。
- 对某些寄存器预设初值（如 `NFC_READY`、`TIREQ_TF0`）。

### 3. 读写路径

```c
read_write(des, typ, val)
  ├─ 若是 DRAM/IRAM/CACHE/EEPROM → loadstore()
  └─ 否则
      ├─ get_register(des4) 获取 ptr
      ├─ 若在 0xF0053000~0xF0054000 → emu_nand_nfc()
      ├─ 否则 → emu_cpu_cop()
      ├─ loadstore(ptr, typ, val)  // 按 typ 做字节/半字/字读写
      └─ 若在 0xF005A020~0xF005A030 → emu_i2c()
```

`typ` 是一套**访问类型枚举**：

```c
WRITE10/11/12/13  // 字节写不同字节位置
WRITE20/22        // 半字写
WRITE40           // 字写
READU10/11/12/13  // 无符号字节读
READS10/...       // 有符号字节读
READU20/22        // 无符号半字读
```

`loadstore()` 根据 typ 对 `*ptr` 做掩码操作，模拟非对齐访问。这是 ARM 模拟器常见手法。

---

## 五、NAND Flash 模拟层（emu_nand_nfc）

这是**整个项目的核心目标**：通过模拟 NFC（NAND Flash Controller）寄存器，还原固件对 NAND 的访问，最终 DUMP 出原始镜像。

### 1. 关键寄存器

| 地址 | 名称 | 作用 |
|---|---|---|
| `0xF0053000` | NFC_CMD | 命令寄存器 |
| `0xF005300C` | NFC_SADDR | 地址寄存器 |
| `0xF0053010` | NFC_WDATA | 数据寄存器 |
| `0xF0053040` | NFC_SDATA | 数据寄存器 |
| `0xF0053050` | NFC_CTRL | 控制寄存器 |
| `0xF0053054` | NFC_DMA_WR | DMA 写 |
| `0xF0053058` | NFC_DMA_RD | DMA 读 |
| `0xF0053060` | NFC_IREQ | 中断请求 |
| `0xF0053064` | NFC_RST | 复位 |

### 2. 命令解析

`emu_nand_nfc()` 里最核心的是 `NFC_CMD` 写入时的分支：

- `0x00` → 读命令，重置 `nfc_addr`
- `0xFF` → 复位，清 `rptr`
- `0x65` → UID 模式，`rptr = uid`
- `0x90` → Read ID
- `0x30` → Read Raw
- `0x80/0x81` → Write Raw
- `0x70` → 状态读取

地址累加逻辑在 `NFC_SADDR`：

```c
case 0xf1: nfc_anum = 0; break; // read id
case 0x90: nfc_anum = 1; break; // read id
case 0x60: nfc_anum = 3; break; // clear page
default:   nfc_anum = 5; break; // others
```

根据命令决定要收几个地址字节，收齐后计算 `nfc_sadd`：

```c
nfc_sadd = *(USHORT*)&nfc_addr_arr[0]        // 列地址
         + (UINT64)*(ULONG*)&nfc_addr_arr[2] * 0x840; // 行地址 * 页大小
```

`0x840 = 2112`，正是 **K9LAG08U0M 的页大小（2048+64）**！所以这个模拟器就是针对我上一轮回答里那颗 NAND 芯片设计的。

### 3. 数据读写

读取数据时：

```c
case 0x0030: nfc_addr = read_value(nfc_addr, ptr, 4); break;
case 0x3030: nfc_addr = read_value(nfc_addr, ptr, 2); break;
```

写入数据时：

```c
case 0x0080:
case 0x0081: nfc_addr = write_value(nfc_addr, val, 4); break;
```

### 4. 镜像后端

```c
partition = _open("D:\\nand_perfect.bin", _O_RDONLY);
```

- 读取时优先查 `wrbin` 缓存（固件写过的扇区），否则从 `nand_perfect.bin` 读。
- 写入时记录到 `wrbin`，并维护一个**目录**：
  ```c
  wrbin[0x40000+2*i+0] = sector;   // 扇区号
  wrbin[0x40000+2*i+1] = offset;   // 数据偏移
  ```

这样模拟器可以：
1. 从真实 NAND 镜像读。
2. 跟踪固件的写入。
3. 最终把 `wrbin` 导出，得到“固件认为已经写过的数据”。

### 5. 快照机制

`dump_snap_and_reinit()` 在每次 NFC 命令变化时，把上一次操作的：

- 命令号
- 控制字
- 块/页地址
- 数据长度
- 前 20 字节数据
- 访问类型（R1/R4/W1/W4）

格式化输出到 `off.txt`。这是**逆向分析 NAND 访问序列的关键日志**。

---

## 六、I2C 模拟层

`emu_i2c()` 模拟 PCF5060x 电源管理芯片，通过 GPIOA 的 SCL/SDA 位操作还原 I2C 时序。

### 1. 状态机

```c
i2c_gdat   // SDA/SCL 电平
i2c_gdir   // 方向
i2c_stat   // 输入/输出状态
i2c_clck   // 时钟计数
i2c_bytn   // 字节计数
```

### 2. 起始/停止条件检测

```c
if((((old_dat << 2) & 12) + (i2c_gdat & 3)) == 13) // SDA_LO while SCL_HI: start
if((((old_dat << 2) & 12) + (i2c_gdat & 3)) ==  7) // SDA_HI while SCL_HI: stop
```

### 3. 从设备响应

根据 I2C 地址和寄存器偏移，返回不同数据：

| 从设备地址 | 寄存器 | 返回内容 |
|---|---|---|
| `0x10/0x02` | INT1~3 | 中断状态 |
| `0x10/0x0A` | RTCSC | RTC |
| `0x10/0x2e` | ADCC1 | 触摸笔状态 |
| `0x10/0x30` | ADCS1 | X 坐标 |
| `0x10/0x31` | ADCS2 | ADC |
| `0x10/0x32` | ADCS3 | Y 坐标 |

触摸坐标由全局 `xMouse, yMouse` 提供，实现**鼠标控制触摸屏**。

---

## 七、LCD / 显示模拟

`emu_cpu_cop()` 里处理 LCD 控制器寄存器：

| 地址 | 寄存器 | 作用 |
|---|---|---|
| `0xF0000000` | CTRL | 控制 |
| `0xF000008C` | I1CTRL | 像素格式 |
| `0xF0000094` | I1SIZE | 尺寸 |
| `0xF0000098` | I1BASE | 帧缓冲基址 |
| `0xF00000A8` | I1OFF | 偏移 |

当写 `I1BASE` 时：

```c
lcd_buffer = (USHORT*)&dram[(*lcd_fb_base_reg & 0x1ffffff)>>2];
```

主循环里按时间点 dump 帧缓冲：

```c
static ULONG fb_thresh[] = {0x4000000,0x9000000,...};
sprintf(fn, fb_n==6?"fb_final.bin":"fb_%02d.bin",fb_n);
fwrite(lcd_buffer,1,320*240*4,f);
```

这样就能**看到固件运行到不同阶段时的屏幕画面**，是分析固件行为最直观的手段。

---

## 八、主循环 RunCode

```c
for(iii=0; *(DWORD*)pRun && iii<iend; iii++)
{
    // 1. 定时器/中断
    if(++usec >= NUMINSTR_PER_USEC) { (*usec_timer)++; }
    if(++tim1 >= timer1_limit) tim1 = 0;
    if(++tim2 >= timer2_limit) tim2 = 0;
    trigger_interrupts(&cpu);

    // 2. 取指 + 执行
    if(cpu.cpsr->T) { ...Thumb... } else { ...ARM... }

    // 3. 反汇编记录
    if(iii + 500000 >= iend) { dis_asm(...); check_for_loop(...); }

    // 4. 阶段性 dump
    //    - 帧缓冲 fb_*.bin
    //    - IP/LR/背景层
    //    - IRAM 代码
    // 5. 定期刷新 UI
    if((iii & 0xffff) == 0) SendMessage(hWnd, WM_COMMAND, 0, 0);
}
```

关键点：

- **`usec_timer` 是“指令数换时间”**：每 `NUMINSTR_PER_USEC` 条指令，虚拟微秒计时器 +1。
- **`tim1/tim2` 是“指令数换定时器中断”**：定时器不是真实时钟，而是按指令条数触发。
- **`iend = 0x7FFFFFFF`**：理论上跑到 21 亿条指令，但实际有 `*(DWORD*)pRun` 控制退出。
- **`iii + 500000 >= iend`**：在接近结束时才开始反汇编记录，避免日志过大。

---

## 九、日志与可视化体系

| 文件 | 内容 |
|---|---|
| `ata.txt` | NFC 寄存器访问流水 |
| `off.txt` | NFC 命令快照 |
| `reg.txt` | GPIO/I2C 访问 |
| `accessed2.txt` | 每个 MMIO 寄存器的写入数据序列 |
| `lcd.txt` | LCD 寄存器写入 + 帧缓冲 dump 时间点 |
| `fbw.txt` | 帧缓冲 CPU 写记录 |
| `lrw.txt` | LR 层写记录 |
| `bg2.txt` | 背景层写记录 |
| `cmem.txt` | 缓存写记录 |
| `dma.txt` | DMA 访问记录 |
| `bgmap.txt` | 背景映射表 |
| `disassembly_cpu.txt` | 反汇编 |
| `fb_*.bin` | 帧缓冲快照 |
| `ip_layer.bin` / `lr_layer.bin` / `bgsrc.bin` | 图层 dump |
| `iram_code.bin` | IRAM 代码 dump |
| `crash.txt` | 崩溃信息 |
| `dsp_count.txt` | DSP 指令计数 |

这是一套**“执行 + 记录 + 可视化”三位一体**的逆向工具链。

---

## 十、崩溃处理

```c
LONG WINAPI CrashVectored(PEXCEPTION_POINTERS ep)
{
    // 记录 native EIP、模块、ARM PC、iii
    fprintf(f, "access=%s badaddr=%p ARMpc=0x%08x iii=%u\n", ...);
}
```

`AddVectoredExceptionHandler` 捕获访问违例、栈溢出、非法指令、特权指令，记录当前 **ARM PC 和指令计数**，方便定位是固件哪一步触发了宿主崩溃。

---

## 十一、架构特点总结

### 优点

1. **动态 MMIO 寄存器池**：不用预定义寄存器，自动发现 + 统计访问，逆向利器。
2. **NAND 模拟 + 镜像后端**：读真实镜像、跟踪写入、导出写入结果，形成完整 DUMP 链路。
3. **指令数驱动的定时器**：简单有效地把“执行进度”映射到“外设时序”。
4. **多层日志**：NFC/I2C/LCD/DMA/帧缓冲全覆盖。
5. **鼠标控制触摸屏**：交互式调试。
6. **崩溃时记录 ARM PC**：定位问题方便。
7. **阶段性 dump**：帧缓冲、图层、IRAM 分时间点保存。

### 缺点

1. **全局变量极多**：`cpu`、`nfc_*`、`i2c_*`、各种 `FILE*` 全在文件作用域，模块耦合严重。
2. **Windows 强耦合**：`SendMessage`、`hWnd`、`GetModuleHandleEx`、`_open` 等，无法跨平台。
3. **硬编码路径**：`D:\\d2.bin`、`D:\\nand_perfect.bin`、`D:\\eeprom.bin`。
4. **返回协议不统一**：`MAGIC`、`0x08`、真实 PC 混用。
5. **无 cache/TLB 模拟**：直接内存访问，DMA 和 CPU 一致性靠人为 hack。
6. **无异常精确性**：SWI/中断处理简化。
7. **文件 I/O 无缓冲**：`fprintf` 频繁调用，性能一般。
8. **魔法数字多**：`0x840`、`0x1ffffff`、`0x4b000`、各地址硬编码。

---

## 十二、和上一轮 NAND 设计的关系

上一轮我给的 **K9LAG08U0M 硬件 DUMP 方案**，是“**从物理层直接读裸片**”。

而这段代码是“**从固件行为层还原 NAND 访问**”：

| 维度 | 硬件 DUMP | 固件模拟 DUMP |
|---|---|---|
| 对象 | 裸 NAND 芯片 | 运行中的固件 |
| 方法 | MCU 模拟 NAND 时序 | 模拟 CPU + NFC 寄存器 |
| 输出 | 原始 2112 字节/页 | 固件“认为”的读写序列 |
| 优点 | 不依赖固件 | 能看到逻辑层 |
| 缺点 | 需要拆焊、时序严格 | 需要固件能跑、模型准确 |

两者结合才是完整方案：

- 用硬件 DUMP 拿到 `nand_perfect.bin`（本代码里正是这个名字）。
- 用本模拟器跑固件，看它怎么解析 NAND、怎么写回、怎么画屏。
- 最终把逻辑层和物理层对齐，得到完整数据恢复。

**所以这段代码本质上是“NAND 镜像的下游消费者 + 固件行为分析器”**，和我上一轮回答是上下游关系。

---

## 十三、总体评价

这是一个**目标明确的专用模拟器/嗅探器**，不是通用 ARM 模拟器。它的架构可以概括为：

```text
ARM/Thumb 解释器
  + 动态 MMIO 寄存器池
  + NAND/I2C/LCD 外设模型
  + 指令数驱动的时间系统
  + 多层日志与阶段性 dump
  = 固件行为逆向平台
```

它的价值不在“模拟得准”，而在“**记录得全**”。作者显然是为了 DUMP 和分析 Cowon D2 固件而写的，所有设计都服务于这个目标：动态寄存器池是为了发现未知外设，NAND 后端是为了还原镜像，日志和帧缓冲 dump 是为了观察行为，崩溃处理是为了定位问题。

如果要现代化，建议：

1. 用 Rust 重写核心（和你之前转换 Thumb 那部分一致）。
2. 把 MMIO 寄存器池抽象成 trait + 插件。
3. 把日志改成结构化事件流（JSON/Protobuf）。
4. 把 NAND 后端抽象成 `BlockDevice`。
5. 把 UI 和核心解耦，用 CLI 或 Web 前端。
