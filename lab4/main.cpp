#include "utils.hpp"
#include "trans.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>
#include <regex>
using namespace std;

#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp> 
using namespace boost::program_options;

string fileName;
int nWidth, nHeight, nPixel, nLength;
char *yuv;
bool benchmark;

void ParseArg(int argc, char *argv[])
{
    options_description opts("YUV-Image Processor Options");

    opts.add_options()
        ("verbose,v", "print more info")
        ("debug,d", "print debug info")
        ("benchmark,b", "benchmark mode")
        ("filename,f", value<string>()->required(), "riscv elf file")
        ("resolution,r", value<string>()->required(), "image resolution (e.g. 1920x1080)")
        ("help,h", "print help info")
        ;
    variables_map vm;
    
    store(parse_command_line(argc, argv, opts), vm);

    if(vm.count("help"))
    {
        cout << opts << endl;
        return;
    }

    if(vm.count("debug"))
    {
        debug = true;
    }

    if(vm.count("verbose"))
    {
        verbose = true;
    }    

    //--filename tmp.txt
    if (vm.count("filename"))
    {
        fileName = vm["filename"].as<string>();
    }
    else
    {
        printf("please use -f to specify the YUV file.\n");
        exit(0);
    }

    if (vm.count("resolution"))
    {
        string r = vm["resolution"].as<string>();
    	regex reg("^([1-9][0-9]{0,5})x([1-9][0-9]{0,5})$");
    	smatch res;

    	if (!regex_match(r, res, reg))
    	{
    		printf("wrong resolution format. (e.g. 1920x1080)\n");
    		exit(0);
    	}

    	nWidth  = boost::lexical_cast<int>(res[1]);
    	nHeight = boost::lexical_cast<int>(res[2]);
    	nPixel  = nWidth * nHeight;
    	nLength = nWidth * nHeight * 3 / 2;
    }
    else
    {
    	printf("please use -r WxH to specify image size.\n");
    	exit(0);
    }

    if (vm.count("benchmark"))
    {
    	benchmark = true;
    }
    else
    {
    	benchmark = false;
    }

    // printf("[verbose]  %s\n", verbose?"on":"off");
    // printf("[debug]    %s\n", debug?"on":"off");
    printf("[benchmark]  %s\n", benchmark?"on":"off");
}

void ReadYUV(const char *file)
{
	FILE *fi = fopen(file, "r");

	if (fi == NULL)
	{
		printf("[Error] Cannot open YUV file '%s'\n", file);
		return;
	}

	yuv = new char[nLength];
	fread(yuv, 1, nLength, fi);
	fclose(fi);
}

void WriteYUV(const char *file, char *data)
{
	FILE *fo = fopen(file, "w");
	fwrite(data, 1, nLength, fo);
	fclose(fo);
}

void Benchmark()
{
	PixelRGB *rgb;
	char *res;

	rgb = YUV2RGB_Basic(yuv, nWidth, nHeight);
	Timer timer;
	for(int a = 1; a < 255; a += 3)
	{
		res = ARGB2YUV_Basic(rgb, nWidth, nHeight, 256);
		delete [] res;
	}
	printf("Basic: %.4f s\n", timer.StepTime());

	for(int a = 1; a < 255; a += 3)
	{
		res = ARGB2YUV_MMX(rgb, nWidth, nHeight, 256);
		delete [] res;
	}
	printf("MMX: %.4f s\n", timer.StepTime());

	//rgb = YUV2RGB_SSE(yuv, nWidth, nHeight);
	for(int a = 1; a < 255; a += 3)
	{
		res = ARGB2YUV_SSE(rgb, nWidth, nHeight, 256);
		delete [] res;
	}
	printf("SSE: %.4f s\n", timer.StepTime());

	//rgb = YUV2RGB_AVX(yuv, nWidth, nHeight);
	for(int a = 1; a < 255; a += 3)
	{
		res = ARGB2YUV_AVX(rgb, nWidth, nHeight, 256);
		delete [] res;
	}
	printf("AVX: %.4f s\n", timer.StepTime());

	if (rgb)
		delete [] rgb;
}

void Process()
{
	PixelRGB *rgb;
	PixelRGB *rgb2;
	char *res;
	char *res2;

	rgb = YUV2RGB_AVX(yuv, nWidth, nHeight);
	Timer timer;
	res = ARGB2YUV_AVX(rgb, nWidth, nHeight, 128);
	printf("AVX: %.4f s\n", timer.StepTime());

	// rgb = YUV2RGB_SSE(yuv, nWidth, nHeight);
	// res2 = ARGB2YUV_SSE(rgb, nWidth, nHeight, 256);
	// printf("SSE: %.4f s\n", timer.StepTime());

	// rgb = YUV2RGB_Basic(yuv, nWidth, nHeight);
	// res2 = ARGB2YUV_Basic(rgb, nWidth, nHeight, 128);
	// printf("Basic: %.4f s\n", timer.StepTime());

	WriteYUV("test.yuv", res);

	// for (int i = 0; i < nLength; ++i)
	// 	if (res[i] != res2[i])
	// 	{
	// 		printf("error: %d %d %d %d %d\n", i, i%nWidth, i/nWidth, (uint8_t)res[i], (uint8_t)res2[i]);
	// 		break;
	// 	}

	if (rgb)
		delete [] rgb;
	if (res)
		delete [] res;
	if (res2)
		delete [] res2;
}

int main(int argc, char *argv[])
{
	ParseArg(argc, argv);
	ReadYUV(fileName.c_str());

	if (benchmark)
		Benchmark();
	else
		Process();

	if (yuv)
		delete [] yuv;

	return 0;
}
