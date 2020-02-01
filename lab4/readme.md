# Lab4 SIMD Optimization

## x86 SIMD 应用程序优化
### 目标
这个项目的目标是首先编写一个基于x86 Basic ISA的对单幅YUV图像进行淡入淡出处理的程序，然后应用SIMD指令对程序进行优化。在Intel这数十年的发展下，已经逐代推出了MMX、SSE、AVX等SIMD指令集，至强服务器CPU和部分顶级消费级处理器甚至搭载了AVX-512指令集，可以对512位宽的向量同时进行操作。

我们的优化目标是针对MMX、SSE、AVX三个不同时代的SIMD指令集来进行程序优化。

### 编译运行
应用程序代码使用了`boost`库，因此编译程序之前首先必须确保其在环境变量中。进入项目文件夹后使用`make`便可构建应用程序。

之后运行`./trans`运行程序，参数如下：

- `-f <filename>` : __必要参数__，指定YUV图像路径
- `-r <resolution>` : __必要参数__，指定YUV图像的解析度（格式如1920x1080）
- `-b` : 运行Benchmark模式，执行85副不同alpha值图像的生成，比较不同指令集时间
- `-h` : 帮助信息

### 优化逻辑

__[Basic ISA]__

对于最基本的一个程序，我们的执行逻辑严格按照实习指导上给出的步骤：

1. 读入一幅YUV格式的图像
2. YUV到RGB转换
3. 计算alpha混合后的RGB值，得到不同亮度的alpha混合
4. 将alpha混合后的图像转换成YUV格式，保存输出文件

其中YUV转RGB这一步骤只需要做1次，而RGB做alpha混合然后转换回YUV需要做85次，因此这里很自然地将YUV2RGB以及ARGB2YUV分成两个函数(位于`trans.hpp`)：

```c++
PixelRGB* YUV2RGB_Basic(const char *yuv, int nWidth, int nHeight);
char* ARGB2YUV_Basic(const PixelRGB *rgb, int nWidth, int nHeight, int alpha);
```

两个函数的实现就是简单地套用了YUV与RGB互转的公式，具体实现可参见`trans.cpp`。有了这两个核心函数就可以简单地实现淡入淡出处理。

__[MMX]__

现在我们用MMX指令集来对原程序进行优化。MMX指令集于1996年提出，最多可以一次性处理__64位__的数据，尽管今天我们的64位处理器上一个通用寄存器就是64位的，但是如果从当时的角度来看，64位宽足够我们一次性处理4个左右的数据。

一个很棘手的问题是，MMX指令集仅能用于整数计算，不提供浮点支持，而我们的公式中都是浮点的计算。因此这里将小数的系数乘上一个较大数转换为整数计算，最后再通过除法/移位转换回去。这样操作会损失一部分精度，但是在测试之后发现实际差距并不明显。具体的转换方法如下，以$Y$值计算为例，原本的公式为：

$$Y = 0.256788 * R + 0.504129 * G + 0.097906 * B + 16$$

这里对于所有系数乘上32768，截取整数部分：

$$Y \approx (8414 * R + 16519 * G + 3208 * B) / 32768 + 16$$

选择乘上32768是为了让移位代替除法加速。

对所有的公式做上面的近似变换即可，ARGB转YUV核心代码如下(完整请见`trans.cpp`)：

```c++
char* ARGB2YUV_MMX(const PixelRGB *rgb, int nWidth, int nHeight, int alpha)
{
    ...
    c_alpha = _mm_set_pi16(1, alpha, alpha, alpha);
    c_srl = _mm_set_pi16(0, 0, 0, 8);

    for (int h = 0; h < nHeight; ++h)
        for (int w = 0; w < nWidth; ++w)
        {
            int off = h*nWidth + w;

            v_rgb = _mm_set_pi16(1, rgb[off].r, rgb[off].g, rgb[off].b);
            v_rgb = _mm_srl_pi16(_m_pmullw(v_rgb, c_alpha), c_srl);

            v_res = _m_pmaddwd(v_rgb, _mm_set_pi16(0, 8414, 16519, 3208));
            tmp = _m_to_int64(v_res);
            y = (((int)tmp + (int)(tmp >> 32)) >> 15) + 16;

            v_res = _m_pmaddwd(v_rgb, _mm_set_pi16(0, -4856, -9535, 14392));
            tmp = _m_to_int64(v_res);
            u = (((int)tmp + (int)(tmp >> 32)) >> 15) + 128;

            v_res = _m_pmaddwd(v_rgb, _mm_set_pi16(0, 14392, -12051, -2340));
            tmp = _m_to_int64(v_res);
            v = (((int)tmp + (int)(tmp >> 32)) >> 15) + 128;

            ...
        }
}
```

__[SSE]__

SSE指令集经过了多次拓展，包括SSE、SSE2、SSE3、SSSE3、SSE4.1、SSE4.2。其支持最大__128位宽__的运算指令，并且加入了浮点运算的支持，这使得我们能够较为简单地对原程序进行修改。

一般来说，循环程序的并行化思路分为两种，一种是循环展开，一种则是针对循环体的特性进行并行设计。循环展开比较粗暴，但是需要对前后循环之间的影响做一定的处理，并要处理余项。而针对特性的并行设计并非每个循环都可以做，比如说关键路径是一条直线显然就是不行的。

在这里可以发现对于RGB转YUV的这个任务中，恰好存在R、G、B和常数四个值，可以直接封装在一个`__m128`结构之中，因此这里先对循环体内部做SSE并行修改。

注意到SSE4.1引入了`_mm_dp_ps`指令，可以在对应向量点乘的同时将结果求和，这更加省略了分开求和的步骤。基于SSE指令集的ARGB转YUV核心代码如下(完整请见`trans.cpp`)：

```c++
char* ARGB2YUV_SSE(const PixelRGB *rgb, int nWidth, int nHeight, int alpha)
{
    ...
    y_arg = _mm_set_ps(16.0, 0.256788, 0.504129, 0.097906);
    u_arg = _mm_set_ps(128.0, -0.148223, -0.290993, 0.439216);
    v_arg = _mm_set_ps(128.0, 0.439216, -0.367788, -0.071427);

    for (int h = 0; h < nHeight; ++h)
        for (int w = 0; w < nWidth; ++w)
        {
            int off = h*nWidth + w;

            float t_alpha = (float)alpha / 256;
            __m128 v_rgb = _mm_mul_ps(_mm_set_ps(1.0, rgb[off].r, rgb[off].g, rgb[off].b),
                                    _mm_set_ps(1.0, t_alpha, t_alpha, t_alpha));
            
            v_res = _mm_dp_ps(v_rgb, y_arg, 0xff);
            y = _mm_extract_ps(v_res, 0x0);
            y = *(float*)(&y);

            v_res = _mm_dp_ps(v_rgb, u_arg, 0xff);
            u = _mm_extract_ps(v_res, 0x0);
            u = *(float*)(&u);

            v_res = _mm_dp_ps(v_rgb, v_arg, 0xff);
            v = _mm_extract_ps(v_res, 0x0);
            v = *(float*)(&v);

            ...
        }
}
```

__[AVX]__

AVX目前阶段分为AVX、AVX2、AVX512，还有未来将会出现的AVX1024。这里我们使用的是AVX1.0指令集，支持__256位宽__数据处理。

优化逻辑与SSE基本一样，但是由于位宽足以放下8个float数据，因此采用循环展开，一次性处理两个像素点，注意由于YUV图像的特殊性，0对齐的情况下连续两个像素点的U、V值是一样的，只需要计算一次即可。

基于AVX指令集的ARGB转YUV核心代码如下(完整请见`trans.cpp`)：

```c++
char* ARGB2YUV_AVX(const PixelRGB *rgb, int nWidth, int nHeight, int alpha)
{
    ...
    y_arg = _mm256_set_ps(16.0, 0.256788, 0.504129, 0.097906,
                            16.0, 0.256788, 0.504129, 0.097906);
    u_arg = _mm256_set_ps(128.0, -0.148223, -0.290993, 0.439216,
                            128.0, -0.148223, -0.290993, 0.439216);
    v_arg = _mm256_set_ps(128.0, 0.439216, -0.367788, -0.071427,
                            128.0, 0.439216, -0.367788, -0.071427);

    for (int h = 0; h < nHeight; ++h)
        for (int w = 0; w < nWidth; w += 2)
        {
            int off = h*nWidth + w;

            float t_alpha = (float)alpha / 256;
            __m256 v_rgb = _mm256_mul_ps(_mm256_set_ps(1.0, rgb[off].r, rgb[off].g, rgb[off].b, 1.0, rgb[off+1].r, rgb[off+1].g, rgb[off+1].b),
                                    		_mm256_set_ps(1.0, t_alpha, t_alpha, t_alpha, 1.0, t_alpha, t_alpha, t_alpha));
            
            v_res = _mm256_dp_ps(v_rgb, y_arg, 0xff);
            y1 = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x1), 0x0);
            y1 = *(float*)(&y1);
            y2 = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x0), 0x0);
            y2 = *(float*)(&y2);

            v_res = _mm256_dp_ps(v_rgb, u_arg, 0xff);
            u = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x0), 0x0);
            u = *(float*)(&u);

            v_res = _mm256_dp_ps(v_rgb, v_arg, 0xff);
            v = _mm_extract_ps(_mm256_extractf128_ps(v_res, 0x0), 0x0);
            v = *(float*)(&v);

            ...
        }
}
```

### 处理结果

__[dem1.yuv]__

alpha值在128下的图像：

![dem1_128](/Users/evernebula/Documents/CS/archlab/lab4/dem1_128.png)

__[dem2.yuv]__

alpha值在128下的图像：

![dem2_128](/Users/evernebula/Documents/CS/archlab/lab4/dem2_128.png)

不同指令集的处理的数据对照基本一样，最多也只会有1左右的误差，而反映在图像上几乎没有差别。

### 效率比较

测试方式是对于每一种指令集优化的代码，运行1次YUV转RGB，85次ARGB转YUV，取20次运行的__平均时间__。测试机器CPU为Intel Core i5-6267U@2.9 GHz，操作系统为Mac OSX。

结果如下，单位秒(s)：

Basic ISA|MMX|SSE|AVX
-|-|-|-
1.3311|1.2216|0.9675|__0.6897__

可以发现，AVX指令集的确是运行最快的，但是实际时间并没有如我们期待的比SSE快2倍，SSE也没有比Basic快很多。这是因为除了SIMD指令外其实还有许多的其它普通指令（比如数组取数的访存），而且数据加载进SIMD指令专用寄存器也要时间，这些都限制了效率表现。

至于MMX比较特殊，因为它是牺牲了精度的结果，而且只有64位宽，在现在的处理器上没有太大的优势，因此提升很小（从某种程度上说甚至更差，因为舍弃了浮点，运算却没快多少）。



## 自定义SIMD指令

### 指令设计

在RISCV SPEC文档中关于Packed SIMD指令的P拓展目前是0.1版本，并没有对指令的定义，只是笼统地简述了预计的实现，而且其中提到用的寄存器还是原先的浮点寄存器，这与本题的要求不符，因此在这里并不是基于P拓展设计，而是自己设计全新的SIMD拓展集。

该SIMD拓展集拥有八个独立的256位SIMD寄存器，命名为v0~v7，。拓展集的指令助记符以v开头，命名基本参考x86-64的AVX指令命名并稍加改动。

__[指令定义]__

| Instruction          | Type | Function                                                |
| -------------------- | ---- | ------------------------------------------------------- |
| __存取指令__ | | |
| vl  rd, offset(rs1)  | I    | 从Mem(R[rs1] + offset)为起始地址读入256位packed数据到rd |
| vs rs2, offset(rs1)  | S    | 将rs2中的256位packed数据写入Mem(R[rs1] + offset)        |
| __packed运算__ | | |
| vaddpb rd, rs1, rs2  | R    | 将rs1与rs2执行8位整数packed加法                         |
| vaddph rd, rs1, rs2  | R    | 将rs1与rs2执行16位整数packed加法                        |
| vaddpw rd, rs1, rs2  | R    | 将rs1与rs2执行32位整数packed加法                        |
| vsubpb rd, rs1, rs2  | R    | 将rs1与rs2执行8位整数packed减法                         |
| vsubph rd, rs1, rs2  | R    | 将rs1与rs2执行16位整数packed减法                        |
| vsubpw rd, rs1, rs2  | R    | 将rs1与rs2执行32位整数packed减法                        |
| vmullpb rd, rs1, rs2 | R    | 将rs1与rs2执行8位整数packed乘法，取低8位                |
| vmulhpb rd, rs1, rs2 | R    | 将rs1与rs2执行8位整数packed乘法，取高8位                |
| vmullph rd, rs1, rs2 | R    | 将rs1与rs2执行16位整数packed乘法，取低16位              |
| vmulhph rd, rs1, rs2 | R    | 将rs1与rs2执行16位整数packed乘法，取高16位              |
| vmullpw rd, rs1, rs2 | R    | 将rs1与rs2执行32位整数packed乘法，取低32位              |
| vmulhpw rd, rs1, rs2 | R    | 将rs1与rs2执行32位整数packed乘法，取高32位              |
| vaddps rd, rs1, rs2  | R    | 将rs1与rs2执行单精度浮点packed加法                      |
| vsubps rd, rs1, rs2  | R    | 将rs1与rs2执行单精度浮点packed减法                      |
| vmulps rd, rs1, rs2  | R    | 将rs1与rs2执行单精度浮点packed乘法                      |
| __饱和计算__ | | |
| vsaddpb rd, rs1, rs2 | R    | 将rs1与rs2执行8位整数packed饱和加法                     |
| vsaddph rd, rs1, rs2 | R    | 将rs1与rs2执行16位整数packed饱和加法                    |
| vsaddpw rd, rs1, rs2 | R    | 将rs1与rs2执行32位整数packed饱和加法                    |
| vssubpb rd, rs1, rs2 | R    | 将rs1与rs2执行8位整数packed饱和减法                     |
| vssubph rd, rs1, rs2 | R    | 将rs1与rs2执行16位整数packed饱和减法                    |
| vssubpw rd, rs1, rs2 | R    | 将rs1与rs2执行32位整数packed饱和减法                    |
| vmaxpb rd, rs1, rs2  | R    | 将rs1与rs2执行8位整数取max                              |
| vmaxph rd, rs1, rs2  | R    | 将rs1与rs2执行16位整数取max                             |
| vmaxpw rd, rs1, rs2  | R    | 将rs1与rs2执行32位整数取max                             |
| vminpb rd, rs1, rs2  | R    | 将rs1与rs2执行8位整数取min                              |
| vminph rd, rs1, rs2  | R    | 将rs1与rs2执行16位整数取min                             |
| vminpw rd, rs1, rs2  | R    | 将rs1与rs2执行32位整数取min                             |
| vmaxps rd, rs1, rs2  | R    | 将rs1与rs2执行单精度浮点取max                           |
| vminps rd, rs1, rs2  | R    | 将rs1与rs2执行单精度浮点取min                           |
| __pack&unpack__ |      |                                                         |
| vpckhb rd, rs1, rs2 | R | 将rs1与rs2从packed 16位整数转换到8位整数存进rd |
| vpckwh rd, rs1, rs2 | R | 将rs1与rs2从packed 32位整数转换到16位整数存进rd |
| vunpcklb rd, rs1, rs2 | R | 将rs1与rs2的高128位以8位大小交替存进rd |
| vunpckhb rd, rs1, rs2 | R | 将rs1与rs2的高128位以8位大小交替存进rd |
| vunpcklh rd, rs1, rs2 | R | 将rs1与rs2的高128位以16位大小交替存进rd |
| vunpckhw rd, rs1, rs2 | R | 将rs1与rs2的高128位以32位大小交替存进rd |
| vunpcklw rd, rs1, rs2 | R | 将rs1与rs2的高128位以32位大小交替存进rd |
| __others__ |  |  |
| vextrb rd, rs1, imm | I | 将rs1中的第imm个数存进rd，8位packed |
| vextrh rd, rs1, imm | I | 将rs1中的第imm个数存进rd，16位packed |
| vextrw rd, rs1, imm | I | 将rs1中的第imm个数存进rd，32位packed |
| vextrs rd, rs1, imm | I | 将rs1中的第imm个数存进rd，单精度浮点packed |
| vmaddps rd, rs1, rs2 | R | 将rs1与rs2按单精度浮点点乘后相加，结果存入rd |

__[指令编码]__

先给出OPCODE的编码：

| I-Type | imm[11:0] | rs1  | funct3 | rd   | opcode               |
| ------ | --------- | ---- | ------ | ---- | -------------------- |
| Bits   | 12        | 5    | 3      | 5    | 7                    |
| vl     | offset    | rs1  | 0x0    | rd   | OP-VL(加载)          |
| vextrb | imm       | rs1  | 0x0    | rd   | OP-VEXTR(截取)       |
| vextrh |           |      | 0x1    |      |                      |
| vextrw |           |      | 0x2    |      |                      |
| vextrs |           |      | 0x4    |      | OP-VEXTR-F(浮点截取) |

| S-Type | imm[11:5] | rs2  | rs1  | funct3 | imm[4:0] | Opcode      |
| ------ | --------- | ---- | ---- | ------ | -------- | ----------- |
| Bits   | 7         | 5    | 5    | 3      | 5        | 7           |
| vs     | imm[11:5] | rs2  | rs1  | 0x0    | imm[4:0] | OP-VS(存储) |

| R-Type   | funct7 | rs2  | rs1  | funct3 | rd   | opcode                         |
| -------- | ------ | ---- | ---- | ------ | ---- | ------------------------------ |
| Bits     | 7      | 5    | 5    | 3      | 5    | 7                              |
| vaddpb   | 0x0    | rs2  | rs1  | 0x0    | rd   | OP-VPCALC(packed计算)          |
| vaddph   |        |      |      | 0x1    |      |                                |
| vaddpw   |        |      |      | 0x2    |      |                                |
| vsubpb   | 0x1    |      |      | 0x0    |      |                                |
| vsubph   |        |      |      | 0x1    |      |                                |
| vsubpw   |        |      |      | 0x2    |      |                                |
| vmullpb  | 0x2    |      |      | 0x0    |      |                                |
| vmullph  |        |      |      | 0x1    |      |                                |
| vmullpw  |        |      |      | 0x2    |      |                                |
| vmulhpb  | 0x3    |      |      | 0x0    |      |                                |
| vmulhph  |        |      |      | 0x1    |      |                                |
| vmulhpw  |        |      |      | 0x2    |      |                                |
| vaddps   | 0x0    |      |      | 0x0    |      | OP-VPCALC-F(packed浮点计算)    |
| vsubps   | 0x1    |      |      | 0x0    |      |                                |
| vmulps   | 0x2    |      |      | 0x0    |      |                                |
| vmaddps  | 0x10   |      |      | 0x0    |      |                                |
| vsaddpb  | 0x0    |      |      | 0x0    |      | OP-VPCALC-S(packed饱和计算)    |
| vsaddph  |        |      |      | 0x1    |      |                                |
| vsasspw  |        |      |      | 0x2    |      |                                |
| vssubpb  | 0x1    |      |      | 0x0    |      |                                |
| vssubph  |        |      |      | 0x1    |      |                                |
| vssubpw  |        |      |      | 0x2    |      |                                |
| vmaxpb   | 0x0    |      |      | 0x0    |      | OP-VPCALC-M(packed大小值运算)  |
| vmaxph   |        |      |      | 0x1    |      |                                |
| vmaxpw   |        |      |      | 0x2    |      |                                |
| vminpb   | 0x1    |      |      | 0x0    |      |                                |
| vminph   |        |      |      | 0x1    |      |                                |
| vminpw   |        |      |      | 0x2    |      |                                |
| vmaxps   | 0x0    |      |      | 0x0    |      | OP-VPCALC-MF(packed浮点大小值) |
| vminps   | 0x1    |      |      | 0x0    |      |                                |
| vpckhb   | 0x0    |      |      | 0x0    |      | OP-VPACK(pack指令)             |
| vpckwh   |        |      |      | 0x1    |      |                                |
| vunpcklb | 0x0    |      |      | 0x0    |      | OP-VUNPCK(unpack指令)          |
| vunpcklh |        |      |      | 0x1    |      |                                |
| vunpcklw |        |      |      | 0x2    |      |                                |
| vunpckhb | 0x1    |      |      | 0x0    |      |                                |
| vunpckhh |        |      |      | 0x1    |      |                                |
| vunpckhw |        |      |      | 0x2    |      |                                |

### 核心函数

以上我们只定义了汇编层级的指令，我们需要一些C语言的API来调用它们，因此先给出一些C语言函数声明：

- `__v256 _simd_set_ps(f0, f1, f2, f3, f4, f5, f6, f7)`: 首先将这些值保存到一个连续的数组中，然后调用`vl`来加载到寄存器中。
- `__v256 _simd_add_ps(a, b)`:调用`vaddps`来实现单精度浮点packed加法（乘法同理）。
- `float _simd_madd_ps(a, b)`:调用`vmaddps`来实现向量点乘后相加。
- `__v256 _simd_maskhi_ps(a)`:调用`vmulps`来实现将低128位置0。
- `__v256 _simd_masklo_ps(a)`:调用`vmulps`来实现将高128位置0。

__[YUV转RGB]__

```c++
PixelRGB* YUV2RGB_RISCV(const char *yuv, int nWidth, int nHeight)
{
	int nPixel, nLength;
	nPixel  = nWidth * nHeight;
	nLength = nWidth * nHeight * 3 / 2;

	PixelRGB *result = new PixelRGB[nPixel];

	__v256 r_arg = _simd_set_ps(0.0, 1.164383, 0.0, 1.596027,
														0.0, 1.164383, 0.0, 1.596027);
	__v256 g_arg = _simd_set_ps(0.0, 1.164383, -0.391762, -0.812968,
														0.0, 1.164383, -0.391762, -0.812968);
	__v256 b_arg = _simd_set_ps(0.0, 1.164383, 2.017232, 0.0,
														0.0, 1.164383, 2.017232, 0.0);

	for (int h = 0; h < nHeight; ++h)
		for (int w = 0; w < nWidth; w += 2)
		{
			int off = h*nWidth + w;

			int y1 = (uint8_t)yuv[off];
			int y2 = (uint8_t)yuv[off+1];
			int u = (uint8_t)yuv[nPixel + (h/2)*(nWidth/2) + (w/2)];
			int v = (uint8_t)yuv[nPixel + nPixel/4 + (h/2)*(nWidth/2) + (w/2)];

			__v256 v_yuv = _simd_set_ps(1.0, y1-16, u-128, v-128,
																1.0, y2-16, u-128, v-128);

			float r1, g1, b1;
			float r2, g2, b2; 
			r1 = _simd_madd_ps(_simd_maskhi(v_yuv), r_arg);
			r2 = _simd_madd_ps(_simd_masklo(v_yuv), r_arg);
     	g1 = _simd_madd_ps(_simd_maskhi(v_yuv), g_arg);
			g2 = _simd_madd_ps(_simd_masklo(v_yuv), g_arg);
      b1 = _simd_madd_ps(_simd_maskhi(v_yuv), b_arg);
			b2 = _simd_madd_ps(_simd_masklo(v_yuv), b_arg);

			result[off] = PixelRGB(clip(r1), clip(g1), clip(b1));
			result[off+1] = PixelRGB(clip(r2), clip(g2), clip(b2));
		}

	return result;
}
```

__[ARGB转YUV]__

```c++
char* ARGB2YUV_RISCV(const PixelRGB *rgb, int nWidth, int nHeight, int alpha)
{
	int nPixel, nLength;
	nPixel  = nWidth * nHeight;
	nLength = nWidth * nHeight * 3 / 2;

	char *result = new char[nLength];

	__v256 y_arg = _simd_set_ps(16.0, 0.256788, 0.504129, 0.097906,
														16.0, 0.256788, 0.504129, 0.097906);
	__v256 u_arg = _simd_set_ps(128.0, -0.148223, -0.290993, 0.439216,
														128.0, -0.148223, -0.290993, 0.439216);
	__v256 v_arg = _simd_set_ps(128.0, 0.439216, -0.367788, -0.071427,
														128.0, 0.439216, -0.367788, -0.071427);

	for (int h = 0; h < nHeight; ++h)
		for (int w = 0; w < nWidth; w += 2)
		{
			int off = h*nWidth + w;

			float t_alpha = (float)alpha / 256;
			__v256 v_rgb = _simd_mul_ps(_simd_set_ps(1.0, rgb[off].r, rgb[off].g, rgb[off].b,
																					1.0, rgb[off+1].r, rgb[off+1].g, rgb[off+1].b),
																	_simd_set_ps(1.0, t_alpha, t_alpha, t_alpha,
																					1.0, t_alpha, t_alpha, t_alpha));
			
			float y1, y2, u, v;
			y1 = _simd_madd_ps(_simd_mask_hi(v_rgb), y_arg);
			y2 = _simd_madd_ps(_simd_mask_lo(v_rgb), y_arg);
			u = _simd_madd_ps(v_rgb, u_arg) / 2;
      v = _simd_madd_ps(v_rgb, v_arg) / 2;

			result[off] = y1;
			result[off+1] = y2;
			result[nPixel + (h/2)*(nWidth/2) + (w/2)] = u;
			result[nPixel + nPixel/4 + (h/2)*(nWidth/2) + (w/2)] = v;
		}

	return result;
}
```

### 性能提升

只需要分析循环内部的代码即可。可以发现对于YUV转RGB，核心部分用`vmaddps`指令来进行乘积和求和，但是由于求和是对全部八个数求和，而这里同时计算了两个像素，因此会把两个像素的值加在一起。所以引入了遮罩函数来分别计算两个像素，单独处理每一个像素。对于一个像素而言，将3个乘法和加法用一条指令解决了，因此这里其实省了(5-1)\*3=12条指令。总共大概节省12\*nPixel的指令数，当然考虑到SIMD加载需要的指令数，实际比这个数应该要小不少。同时考虑到不同指令周期不同，实际的性能差异大概会小于2倍。不过这个部分只会调用一次影响不会很大。

对于ARGB转YUV，会调用85次。一次性计算两个像素，首先用`vmulps`来得到alpha混合的RGB，然后同样用`vmaddps`来进行求乘积和的操作，对于Y可以按上面方法做，而U、V由于相邻像素一样，只需要做一遍。节省指令数是(6-1)\*nPixel(alpha混合) + (5-1)\*nPixel(Y计算) + (10-1)\*2\*nPixel(U、V计算) = 27\*nPixel。这个部分的实际性能差异会比上面大，但是也不会超过2~3倍。

总共的指令节省数大约为(27*85+12)\*nPixel，这个数大约是在1e10的量级上。性能提升应该与AVX指令集差不太多，但是由于并没有AVX那么完善，可能会慢一点，因此大概的速度应该为原来的两倍左右。