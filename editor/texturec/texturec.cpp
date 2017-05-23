/*
 * Copyright 2011-2017 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include <stdio.h>
#include <bx/allocator.h>
#include <bx/readerwriter.h>
#include <bx/endian.h>

#include <bimg/decode.h>
#include <bimg/encode.h>

#if 0
#	define DBG(_format, ...) fprintf(stderr, "" _format "\n", ##__VA_ARGS__)
#else
#	define DBG(...) BX_NOOP()
#endif // DEBUG

#include <bx/bx.h>
#include <bx/commandline.h>
#include <bx/crtimpl.h>
#include <bx/uint32_t.h>

#define BIMG_TEXTUREC_VERSION_MAJOR 1
#define BIMG_TEXTUREC_VERSION_MINOR 2

struct Options
{
	Options()
		: maxSize(UINT32_MAX)
		, edge(0.0f)
		, format(bimg::TextureFormat::Count)
		, mips(false)
		, normalMap(false)
		, iqa(false)
		, sdf(false)
	{
	}

	void dump()
	{
		DBG("Options:\n"
			"\t  maxSize: %d\n"
			"\t     edge: %f\n"
			"\t   format: %s\n"
			"\t     mips: %s\n"
			"\tnormalMap: %s\n"
			"\t      iqa: %s\n"
			"\t      sdf: %s\n"
			, maxSize
			, edge
			, bimg::getName(format)
			, mips      ? "true" : "false"
			, normalMap ? "true" : "false"
			, iqa       ? "true" : "false"
			, sdf       ? "true" : "false"
			);
	}

	uint32_t maxSize;
	float edge;
	bimg::TextureFormat::Enum format;
	bool mips;
	bool normalMap;
	bool iqa;
	bool sdf;
};

bimg::ImageContainer* convert(bx::AllocatorI* _allocator, const void* _inputData, uint32_t _inputSize, const Options& _options)
{
	bool succeeded = true;
	const uint8_t* inputData = (uint8_t*)_inputData;

	bimg::ImageContainer* output = NULL;
	bimg::ImageContainer* input  = bimg::imageParse(_allocator, inputData, _inputSize);

	if (NULL != input)
	{
		const bimg::TextureFormat::Enum inputFormat  = input->m_format;
		      bimg::TextureFormat::Enum outputFormat = input->m_format;

		if (bimg::TextureFormat::Count != _options.format)
		{
			outputFormat = _options.format;
		}

		const bimg::ImageBlockInfo&  inputBlockInfo  = bimg::getBlockInfo(inputFormat);
		const bimg::ImageBlockInfo&  outputBlockInfo = bimg::getBlockInfo(outputFormat);
		const uint32_t blockWidth  = outputBlockInfo.blockWidth;
		const uint32_t blockHeight = outputBlockInfo.blockHeight;
		const uint32_t minBlockX   = outputBlockInfo.minBlockX;
		const uint32_t minBlockY   = outputBlockInfo.minBlockY;
		uint32_t outputWidth  = bx::uint32_max(blockWidth  * minBlockX, ( (input->m_width  + blockWidth  - 1) / blockWidth )*blockWidth);
		uint32_t outputHeight = bx::uint32_max(blockHeight * minBlockY, ( (input->m_height + blockHeight - 1) / blockHeight)*blockHeight);

		if (outputWidth  > _options.maxSize
		||  outputHeight > _options.maxSize)
		{
			if (outputWidth > outputHeight)
			{
				outputHeight = outputHeight * _options.maxSize / outputWidth;
				outputWidth  = _options.maxSize;
			}
			else
			{
				outputWidth  = outputWidth * _options.maxSize / outputHeight;
				outputHeight = _options.maxSize;
			}
		}

		const bool needResize = false
			|| input->m_width  != outputWidth
			|| input->m_height != outputHeight
			;

		const bool passThru = true
			&& inputFormat == outputFormat
			&& !needResize
			&& (1 < input->m_numMips) == _options.mips
			;

		if (needResize)
		{
			bimg::ImageContainer* src = bimg::imageConvert(_allocator, bimg::TextureFormat::RGBA32F, *input);

			bimg::ImageContainer* dst = bimg::imageAlloc(
				  _allocator
				, bimg::TextureFormat::RGBA32F
				, uint16_t(outputWidth)
				, uint16_t(outputHeight)
				, 1
				, input->m_numLayers
				, input->m_cubeMap
				, false
				);

			bimg::imageResizeRgba32fLinear(dst, src);

			bimg::imageFree(src);
			bimg::imageFree(input);

			input = bimg::imageConvert(_allocator, inputFormat, *dst);
			bimg::imageFree(dst);
		}

		if (passThru)
		{
			output = bimg::imageConvert(_allocator, outputFormat, *input);
			bimg::imageFree(input);
			return output;
		}

		output = bimg::imageAlloc(
			  _allocator
			, outputFormat
			, uint16_t(input->m_width)
			, uint16_t(input->m_height)
			, uint16_t(input->m_depth)
			, input->m_numLayers
			, input->m_cubeMap
			, _options.mips
			);

		const uint8_t  numMips  = output->m_numMips;
		const uint16_t numSides = output->m_numLayers * (output->m_cubeMap ? 6 : 1);

		for (uint16_t side = 0; side < numSides && succeeded; ++side)
		{
			bimg::ImageMip mip;
			if (bimg::imageGetRawData(*input, side, 0, input->m_data, input->m_size, mip) )
			{
				bimg::ImageMip dstMip;
				bimg::imageGetRawData(*output, side, 0, output->m_data, output->m_size, dstMip);
				uint8_t* dstData = const_cast<uint8_t*>(dstMip.m_data);

				void* temp = NULL;

				if (_options.normalMap)
				{
					uint32_t size = bimg::imageGetSize(
						  NULL
						, uint16_t(dstMip.m_width)
						, uint16_t(dstMip.m_height)
						, 0
						, false
						, false
						, 1
						, bimg::TextureFormat::RGBA32F
						);
					temp = BX_ALLOC(_allocator, size);
					float* rgba = (float*)temp;
					float* rgbaDst = (float*)BX_ALLOC(_allocator, size);

					bimg::imageDecodeToRgba32f(_allocator
						, rgba
						, mip.m_data
						, dstMip.m_width
						, dstMip.m_height
						, dstMip.m_width*16
						, mip.m_format
						);

					if (bimg::TextureFormat::BC5 != mip.m_format)
					{
						for (uint32_t yy = 0; yy < mip.m_height; ++yy)
						{
							for (uint32_t xx = 0; xx < mip.m_width; ++xx)
							{
								const uint32_t offset = (yy*mip.m_width + xx) * 4;
								float* inout = &rgba[offset];
								inout[0] = inout[0] * 2.0f - 1.0f;
								inout[1] = inout[1] * 2.0f - 1.0f;
								inout[2] = inout[2] * 2.0f - 1.0f;
								inout[3] = inout[3] * 2.0f - 1.0f;
							}
						}
					}

					bimg::imageRgba32f11to01(rgbaDst
						, dstMip.m_width
						, dstMip.m_height
						, dstMip.m_width*16
						, rgba
						);

					bimg::imageEncodeFromRgba32f(_allocator
						, dstData
						, rgbaDst
						, dstMip.m_width
						, dstMip.m_height
						, outputFormat
						);

					for (uint8_t lod = 1; lod < numMips; ++lod)
					{
						bimg::imageRgba32fDownsample2x2NormalMap(rgba
							, dstMip.m_width
							, dstMip.m_height
							, dstMip.m_width*16
							, rgba
							);

						bimg::imageRgba32f11to01(rgbaDst
							, dstMip.m_width
							, dstMip.m_height
							, dstMip.m_width*16
							, rgba
							);

						bimg::imageGetRawData(*output, side, lod, output->m_data, output->m_size, dstMip);
						dstData = const_cast<uint8_t*>(dstMip.m_data);

						bimg::imageEncodeFromRgba32f(_allocator
							, dstData
							, rgbaDst
							, dstMip.m_width
							, dstMip.m_height
							, outputFormat
							);
					}

					BX_FREE(_allocator, rgbaDst);
				}
				else if (!bimg::isCompressed(input->m_format)
					 &&  8 != inputBlockInfo.rBits)
				{
					uint32_t size = bimg::imageGetSize(
						  NULL
						, uint16_t(dstMip.m_width)
						, uint16_t(dstMip.m_height)
						, 0
						, false
						, false
						, 1
						, bimg::TextureFormat::RGBA32F
						);
					temp = BX_ALLOC(_allocator, size);
					float* rgba32f = (float*)temp;
					float* rgbaDst = (float*)BX_ALLOC(_allocator, size);

					bimg::imageDecodeToRgba32f(_allocator
						, rgba32f
						, mip.m_data
						, mip.m_width
						, mip.m_height
						, mip.m_width*16
						, mip.m_format
						);

					bimg::imageEncodeFromRgba32f(_allocator
						, dstData
						, rgba32f
						, dstMip.m_width
						, dstMip.m_height
						, outputFormat
						);

					if (1 < numMips)
					{
						bimg::imageRgba32fToLinear(rgba32f
							, mip.m_width
							, mip.m_height
							, mip.m_width*16
							, rgba32f
							);

						for (uint8_t lod = 1; lod < numMips; ++lod)
						{
							bimg::imageRgba32fLinearDownsample2x2(rgba32f
								, dstMip.m_width
								, dstMip.m_height
								, dstMip.m_width*16
								, rgba32f
								);

							bimg::imageGetRawData(*output, side, lod, output->m_data, output->m_size, dstMip);
							dstData = const_cast<uint8_t*>(dstMip.m_data);

							bimg::imageRgba32fToGamma(rgbaDst
								, mip.m_width
								, mip.m_height
								, mip.m_width*16
								, rgba32f
								);

							succeeded = bimg::imageEncodeFromRgba32f(_allocator
								, dstData
								, rgbaDst
								, dstMip.m_width
								, dstMip.m_height
								, outputFormat
								);

							if (!succeeded)
							{
								break;
							}

						}
					}

					BX_FREE(_allocator, rgbaDst);
				}
				else
				{
					uint32_t size = bimg::imageGetSize(
						  NULL
						, uint16_t(dstMip.m_width)
						, uint16_t(dstMip.m_height)
						, 0
						, false
						, false
						, 1
						, bimg::TextureFormat::RGBA8
						);
					temp = BX_ALLOC(_allocator, size);
					uint8_t* rgba = (uint8_t*)temp;

					bimg::imageDecodeToRgba8(rgba
						, mip.m_data
						, mip.m_width
						, mip.m_height
						, mip.m_width*4
						, mip.m_format
						);

					void* ref = NULL;
					if (_options.iqa)
					{
						ref = BX_ALLOC(_allocator, size);
						bx::memCopy(ref, rgba, size);
					}

					bimg::imageGetRawData(*output, side, 0, output->m_data, output->m_size, dstMip);
					dstData = const_cast<uint8_t*>(dstMip.m_data);
					bimg::imageEncodeFromRgba8(dstData, rgba, dstMip.m_width, dstMip.m_height, outputFormat);

					for (uint8_t lod = 1; lod < numMips; ++lod)
					{
						bimg::imageRgba8Downsample2x2(rgba
							, dstMip.m_width
							, dstMip.m_height
							, dstMip.m_width*4
							, rgba
							);

						bimg::imageGetRawData(*output, side, lod, output->m_data, output->m_size, dstMip);
						dstData = const_cast<uint8_t*>(dstMip.m_data);

						bimg::imageEncodeFromRgba8(dstData
							, rgba
							, dstMip.m_width
							, dstMip.m_height
							, outputFormat
							);
					}

					if (NULL != ref)
					{
						bimg::imageDecodeToRgba8(rgba
							, output->m_data
							, mip.m_width
							, mip.m_height
							, mip.m_width*mip.m_bpp/8
							, outputFormat
							);

						float result = bimg::imageQualityRgba8(
							  ref
							, rgba
							, uint16_t(mip.m_width)
							, uint16_t(mip.m_height)
							);

						printf("%f\n", result);

						BX_FREE(_allocator, ref);
					}
				}

				BX_FREE(_allocator, temp);
			}
		}

		bimg::imageFree(input);
	}

	if (!succeeded
	&&  NULL != output)
	{
		bimg::imageFree(output);
		output = NULL;
	}

	return output;
}

void help(const char* _error = NULL)
{
	if (NULL != _error)
	{
		fprintf(stderr, "Error:\n%s\n\n", _error);
	}

	fprintf(stderr
		, "texturec, bgfx texture compiler tool, version %d.%d.%d.\n"
		  "Copyright 2011-2017 Branimir Karadzic. All rights reserved.\n"
		  "License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause\n\n"
		, BIMG_TEXTUREC_VERSION_MAJOR
		, BIMG_TEXTUREC_VERSION_MINOR
		, BIMG_API_VERSION
		);

	fprintf(stderr
		, "Usage: texturec -f <in> -o <out> [-t <format>]\n"

		  "\n"
		  "Supported input file types:\n"
		  "    *.png                  Portable Network Graphics\n"
		  "    *.tga                  Targa\n"
		  "    *.dds                  Direct Draw Surface\n"
		  "    *.ktx                  Khronos Texture\n"
		  "    *.pvr                  PowerVR\n"

		  "\n"
		  "Options:\n"
		  "  -h, --help               Help.\n"
		  "  -v, --version            Version information only.\n"
		  "  -f <file path>           Input file path.\n"
		  "  -o <file path>           Output file path (file will be written in KTX format).\n"
		  "  -t <format>              Output format type (BC1/2/3/4/5, ETC1, PVR14, etc.).\n"
		  "  -m, --mips               Generate mip-maps.\n"
		  "  -n, --normalmap          Input texture is normal map.\n"
		  "      --sdf <edge>         Compute SDF texture.\n"
		  "      --iqa                Image Quality Assesment\n"
		  "      --max <max size>     Maximum width/height (image will be scaled down and\n"
		  "                           aspect ratio will be preserved.\n"
		  "      --as <extension>     Save as.\n"

		  "\n"
		  "For additional information, see https://github.com/bkaradzic/bgfx\n"
		);
}

int compile_texture(int _argc, const char* _argv[])
{
	bx::CommandLine cmdLine(_argc, _argv);

	if (cmdLine.hasArg('v', "version") )
	{
		fprintf(stderr
			, "texturec, bgfx texture compiler tool, version %d.%d.%d.\n"
			, BIMG_TEXTUREC_VERSION_MAJOR
			, BIMG_TEXTUREC_VERSION_MINOR
			, BIMG_API_VERSION
			);
		return EXIT_SUCCESS;
	}

	if (cmdLine.hasArg('h', "help") )
	{
		help();
		return EXIT_FAILURE;
	}

	const char* inputFileName = cmdLine.findOption('f');
	if (NULL == inputFileName)
	{
		help("Input file must be specified.");
		return EXIT_FAILURE;
	}

	const char* outputFileName = cmdLine.findOption('o');
	if (NULL == outputFileName)
	{
		help("Output file must be specified.");
		return EXIT_FAILURE;
	}

	const char* saveAs = cmdLine.findOption("as");
	saveAs = NULL == saveAs ? bx::strFindI(outputFileName, ".ktx") : saveAs;
	if (NULL == saveAs)
	{
		help("Output file format must be specified.");
		return EXIT_FAILURE;
	}

	Options options;

	const char* edgeOpt = cmdLine.findOption("sdf");
	if (NULL != edgeOpt)
	{
		options.sdf  = true;
		options.edge = (float)atof(edgeOpt);
	}

	options.mips      = cmdLine.hasArg('m',  "mips");
	options.normalMap = cmdLine.hasArg('n',  "normalmap");
	options.iqa       = cmdLine.hasArg('\0', "iqa");

	const char* maxSize = cmdLine.findOption("max");
	if (NULL != maxSize)
	{
		options.maxSize = atoi(maxSize);
	}

	options.format = bimg::TextureFormat::Count;
	const char* type = cmdLine.findOption('t');
	if (NULL != type)
	{
		options.format = bimg::getFormat(type);

		if (!bimg::isValid(options.format) )
		{
			help("Invalid format specified.");
			return EXIT_FAILURE;
		}
	}

	bx::CrtFileReader reader;
	if (!bx::open(&reader, inputFileName) )
	{
		help("Failed to open input file.");
		return EXIT_FAILURE;
	}

	bx::CrtAllocator allocator;

	uint32_t inputSize = (uint32_t)bx::getSize(&reader);
	uint8_t* inputData = (uint8_t*)BX_ALLOC(&allocator, inputSize);

	bx::Error err;
	bx::read(&reader, inputData, inputSize, &err);
	bx::close(&reader);

	if (!err.isOk() )
	{
		help("Failed to read input file.");
		return EXIT_FAILURE;
	}

	bimg::ImageContainer* output = convert(&allocator, inputData, inputSize, options);

	BX_FREE(&allocator, inputData);

	if (NULL != output)
	{
		bx::CrtFileWriter writer;
		if (bx::open(&writer, outputFileName) )
		{
			if (NULL != bx::strFindI(saveAs, "ktx") )
			{
				bimg::imageWriteKtx(&writer, *output, output->m_data, output->m_size);
			}

			bx::close(&writer);
		}
		else
		{
			help("Failed to open output file.");
			return EXIT_FAILURE;
		}

		bimg::imageFree(output);
	}
	else
	{
		help("No output generated.");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}