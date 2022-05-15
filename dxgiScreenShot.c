/**
 * DXGI based Screen capture tool. This is used to take a single screen shot and save it too a file.
 */
#define COBJMACROS
#define INITGUID


#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <d3dcommon.h>
#include <Windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


#include "inih/ini.h"
#include "dxgiScreenShot.h"
#include "options.h"


#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof(*(arr)))
#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
#define LOG(data_to_be_printed) printf("FILE=%s::LINE=%d::DATA=%s\n", __FILE__, __LINE__, data_to_be_printed)
#define DEBUGLOGGER(data_to_be_printed) {   \
    if (DEBUG_ENABLED == 1) {               \
        LOG(data_to_be_printed);            \
    }                                       \
}
#define returnIfFailure(hrNum, num, to_be_printed) {        \
        if (FAILED(hrNum)) {                                \
            DEBUGLOGGER(to_be_printed);                     \
            free(this);                                     \
            if (DEBUG_ENABLED == 1) {                       \
                printf("\nPress Enter too continue... \n"); \
                puts(tmpfileName);                          \
            }                                               \
            return num;                                     \
    }                                                       \
}

// Feature levels supported
static const D3D_FEATURE_LEVEL win10featureLevels[] =
{
D3D_FEATURE_LEVEL_12_1,
D3D_FEATURE_LEVEL_12_0,
D3D_FEATURE_LEVEL_11_1,
D3D_FEATURE_LEVEL_11_0,
D3D_FEATURE_LEVEL_10_1,
D3D_FEATURE_LEVEL_10_0,
D3D_FEATURE_LEVEL_9_3,
D3D_FEATURE_LEVEL_9_2,
D3D_FEATURE_LEVEL_9_1
};


struct iface
{
    HDESK                      desktop;
    IDXGIFactory1            * factory;
    IDXGIAdapter1            * adapter;
    IDXGIOutput              * output;
    IDXGIOutput1             * output1;
    ID3D11Device             * device;
    ID3D11DeviceContext      * deviceContext;
    D3D_FEATURE_LEVEL          featureLevel;
    IDXGIOutputDuplication   * dup;
    ID3D11Texture2D          * AcquiredDesktopImage;
    ID3D11Texture2D          * GDIImage;
    ID3D11Texture2D          * DestImage;
    IDXGIResource            * DesktopResource;
    IDXGISurface1            * DXGISurface;
};

static const unsigned int featureLevelCount = ARRAY_LENGTH(win10featureLevels);

static struct iface * this    = NULL;


static struct Option options[] =
{
    {
        .name           = "configFile",
        .description    = "The file to read the configuration from",
        .type           = OPTION_TYPE_STRING,
        .value.x_string = NULL,
    },
    {
        .name           = "fileName",
        .description    = "The filename of the screen catpture image.",
        .type           = OPTION_TYPE_STRING,
        .value.x_string = "ScreenShot.bmp",
    },
    {
        .name           = "drawCursor",
        .description    = "Boolean flag. If provided it will draw the cursor.",
        .type           = OPTION_TYPE_BOOL,
        .value.x_bool   = 0,
    },
    {
        .name           = "debug",
        .description    = "Boolean flag. If provided it will enable debug output too the console",
        .type           = OPTION_TYPE_BOOL,
        .value.x_bool   = 0,
    },
    {
        .name           = "showImage",
        .description    = "Boolean flag. Automatically show imagine if present.",
        .type           = OPTION_TYPE_BOOL,
        .value.x_bool = 0,
    },
    {0}
};


static void printHelp() {
    printf(
        "Usage: dxgiScreenShot.exe [OPTIONS]\n"
        "Takes a screen shot using DXGI Desktop Duplication and D3D11 APIs. Saves the file in Users Pictures directory.\n "
        "/fileName [Filename]: Can specify the name of the screenshot\n "
        "/configFile [dxgiScreenShot.ini]: Can specify the name including path (working directory if not specified) of the .ini configuration file\n "
        "/drawCursor: Boolean flag. If provided it will draw the cursor. If not provided the cursor will not be drawn.\n "
        "/showImage: Boolean flag. If provided it will open the image once it is finished coping it to disk.\n "
        "/debug: Boolean flag. If provied it will enable logging too std out. \n"
    );
}


static int StringEndsWith(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}


int main(int argc, char *argv[]){

    // Automatically hide popup console.
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    
    // List of variables
    HRESULT hr; // The Windows API result code. This will be used to record errors.
    int errorNum = 0; //Used to log the exit code.

    //Flags this is pulled from the Options struct and is either the default value, a value from an ini config file, arguments too the script.
    int8_t CURSOR_FLAG = 0;
    int8_t DEBUG_ENABLED = 0;
    int8_t SHOW_IMAGE_FLAG = 0;
    CHAR CONFIGFILE[_MAX_FNAME] = "dxgiScreenShot.ini";
    CHAR tmpfileName[_MAX_FNAME];
    CHAR fileName[_MAX_FNAME];
    CHAR fileName_date[_MAX_FNAME];
    CHAR * fileName_p = fileName;
    strcpy(fileName, "\\"); //Start fileName with a \
    
    SYSTEMTIME t; // Windows API time struct
    GetLocalTime(&t); // Get current time
    snprintf(fileName_date, _MAX_FNAME-3, "_%d-%d-%d_%d%d%d%d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);

    // Three variables used for the looping through arguments and options.
    int8_t argNum = 0;
    Option * pOption = NULL;
    CHAR optionName[_MAX_FNAME];

    if (argc > 254) {
        LOG("Too many arguments... cannot process exiting");
        return -__LINE__;
    }

    //First loop through to find if a config file has been specified.
    for (argNum = 0; argNum < argc; ++argNum) {
        if (strcmp(argv[argNum], "/configFile") == 0) {
            strcpy_s(CONFIGFILE, _MAX_FNAME, argv[++argNum]);
            break;
        }
    }

    /* 
    * The default config file is located in the working directory of the exe and is named dxgiScreenShot.ini that will be used but it could be 
    * overridden by passing a blank. This checks for that then attempts to load the config file if it has one.
    */
    if (strlen(CONFIGFILE) > 4) {
        if (ini_parse(CONFIGFILE, handler, options) < 0) {
            LOG("Could not load ini configuration file.");
        }
    }

    for (argNum = 0; argNum < argc; ++argNum) {
        if (strcmp(argv[argNum], "/help") == 0) {
            printHelp();
            return errorNum;
        } else if (strcmp(argv[argNum], "/fileName") == 0) {
            strcpy_s(optionName, _MAX_FNAME, "fileName");
            pOption = find_option(optionName, options);
            pOption->value.x_string = argv[++argNum];
        } else if (strcmp(argv[argNum], "/drawCursor") == 0) {
            strcpy_s(optionName, _MAX_FNAME, "drawCursor");
            pOption = find_option(optionName, options);
            pOption->value.x_bool = true;
        } else if (strcmp(argv[argNum], "/debug") == 0) {
            strcpy_s(optionName, _MAX_FNAME, "debug");
            pOption = find_option(optionName, options);
            pOption->value.x_bool = true;
        } else if (strcmp(argv[argNum], "/showImage") == 0) {
            strcpy_s(optionName, _MAX_FNAME, "showImage");
            pOption = find_option(optionName, options);
            pOption->value.x_bool = true;
        }
        pOption = NULL;
    }

    for(argNum = 0; options[argNum].type != OPTION_TYPE_NONE; ++argNum) {
        if (strcmp(options[argNum].name, "fileName") == 0) {
            strcpy(fileName_p+1,options[argNum].value.x_string);
        } else if (strcmp(options[argNum].name, "drawCursor") == 0) {
            CURSOR_FLAG = options[argNum].value.x_bool;
        } else if  (strcmp(options[argNum].name, "debug") == 0) {
            DEBUG_ENABLED = options[argNum].value.x_bool;
        } else if  (strcmp(options[argNum].name, "showImage") == 0) {
            SHOW_IMAGE_FLAG = options[argNum].value.x_bool;
        }
    }

    printf("SHOW_IMAGE_FLAG = %d", SHOW_IMAGE_FLAG);

    if (strlen(fileName) == 1) {
        snprintf(tmpfileName, _MAX_FNAME, "ScreenShot%s.bmp", fileName_date);
        strcpy(fileName_p+1, tmpfileName);
    } else {
        if (StringEndsWith(fileName, ".bmp") == 1) {
            CHAR * end = strrchr(fileName, '.');
            *end = '\0';
        }
        snprintf(tmpfileName, _MAX_FNAME, "%s%s.bmp", fileName, fileName_date);
        strcpy(fileName_p+1, tmpfileName);
    }

    /* DXGI setup vars */
    DXGI_OUTPUT_DESC OutputDesc;
    DXGI_OUTDUPL_DESC OutputDuplDesc;
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;
    this = calloc(1, sizeof(*this));
    UINT Output = 0;


    DEBUGLOGGER("Trying to create device");
    hr = D3D11CreateDevice(
            NULL,
            D3D_DRIVER_TYPE_HARDWARE,
            NULL,
            D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
            win10featureLevels, 
            featureLevelCount,
            D3D11_SDK_VERSION,
            &this->device,
            &this->featureLevel,
            &this->deviceContext);

    returnIfFailure(hr, errorNum - __LINE__, "Failed to create device");

    // Get DXGI device
    IDXGIDevice1 * dxgi;
    hr = ID3D11Device_QueryInterface(this->device, &IID_IDXGIDevice1, (void **)&dxgi);
    IDXGIDevice1_SetMaximumFrameLatency(dxgi, 1);
    IDXGIDevice1_Release(dxgi);
    
    returnIfFailure(hr, errorNum - __LINE__, "Failed to get DXGI device");

    // Get DXGI adapter
    hr = IDXGIDevice1_GetParent(dxgi, &IID_IDXGIAdapter, (void **)&this->adapter);
    
    returnIfFailure(hr, errorNum - __LINE__, "Failed to get DXGI adapter");

    // Get output
    hr = IDXGIAdapter1_EnumOutputs(this->adapter, Output, &this->output);
    ID3D11Device_Release(this->adapter);
    hr = IDXGIOutput1_GetDesc(this->output, &OutputDesc);
    
    returnIfFailure(hr, errorNum - __LINE__, "Failed to get output");

    // QI for Output 1
    hr = ID3D11Device_QueryInterface(this->output, &IID_IDXGIOutput1, (void **)&this->output1);
    ID3D11Device_Release(this->output);
    
    returnIfFailure(hr, errorNum - __LINE__, "Failed to get output 1");

    // Create desktop duplication
    hr = IDXGIOutput1_DuplicateOutput(this->output1, (IUnknown *)this->device, &this->dup);
    IDXGIOutput1_Release(this->output1);
    
    returnIfFailure(hr, errorNum - __LINE__, "Failed to create desktop duplication");

    IDXGIOutputDuplication_GetDesc(this->dup, &OutputDuplDesc);

    
    D3D11_TEXTURE2D_DESC desc;

    //Create GUI drawing texture
    if (CURSOR_FLAG == 1) {
        desc.Width = OutputDuplDesc.ModeDesc.Width;
        desc.Height = OutputDuplDesc.ModeDesc.Height;
        desc.Format = OutputDuplDesc.ModeDesc.Format;
        desc.ArraySize = 1;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.MipLevels = 1;
        desc.CPUAccessFlags = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;

        hr = ID3D11Device_CreateTexture2D(this->device, &desc, NULL, &this->GDIImage);

        returnIfFailure(hr, errorNum - __LINE__, "Failed to create GUI drawing texture");
    }


    // Create CPU access texture
    desc.Width = OutputDuplDesc.ModeDesc.Width;
    desc.Height = OutputDuplDesc.ModeDesc.Height;
    desc.Format = OutputDuplDesc.ModeDesc.Format;
    desc.ArraySize = 1;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.MipLevels = 1;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    desc.Usage = D3D11_USAGE_STAGING;

    hr = ID3D11Device_CreateTexture2D(this->device, &desc, NULL, &this->DestImage);
    
    returnIfFailure(hr, errorNum - __LINE__, "Failed to create CPU access texture");

    //Capture Frame
    int TryCount = 4;

    do {
        Sleep(10);

        hr = IDXGIOutputDuplication_AcquireNextFrame(this->dup, 100, &FrameInfo, &this->DesktopResource);

        if (FrameInfo.LastPresentTime.QuadPart == 0) {
            IDXGIResource_Release(this->DesktopResource);
            continue;
        }

        if (SUCCEEDED(hr))
            break;
        
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            continue;
        }
        else if (FAILED(hr))
            break;
    } while(--TryCount > 0);

    returnIfFailure(hr, errorNum - __LINE__, "Failed to Acquire Next Frame");

    // QI for ID3D11Texture2D
    hr = IDXGIResource_QueryInterface(this->DesktopResource, &IID_ID3D11Texture2D, (void **)&this->AcquiredDesktopImage);
    IDXGIResource_Release(this->DesktopResource);
    
    returnIfFailure(hr, errorNum - __LINE__, "Failed to QI for ID3D11Texture2D");

    // Draw Curser if flag set
    if (CURSOR_FLAG == 1) {
        ID3D11DeviceContext_CopyResource(this->deviceContext, (ID3D11Resource *)this->GDIImage, (ID3D11Resource *)this->AcquiredDesktopImage);

        hr = ID3D11Texture2D_QueryInterface(this->GDIImage, &IID_IDXGISurface1, (void **)&this->DXGISurface);

        returnIfFailure(hr, errorNum - __LINE__, "Failed to Query Texture interface for drawing the curser");

        CURSORINFO Cursor_Info = { 0 };

        Cursor_Info.cbSize = sizeof(Cursor_Info);

        WINBOOL c_results = GetCursorInfo(&Cursor_Info);
        if (c_results == 0) {
            DEBUGLOGGER("FAILED to get cursor info");
            free(this);
            return __LINE__;
        }

        if (Cursor_Info.flags == CURSOR_SHOWING) {
            DEBUGLOGGER("Succeeded at getting cursor info and cursor is showing");
            HDC cursor_hdc;

            IDXGISurface1_GetDC(this->DXGISurface, FALSE, &cursor_hdc);

            c_results = DrawIconEx(
                            cursor_hdc, 
                            Cursor_Info.ptScreenPos.x, 
                            Cursor_Info.ptScreenPos.y, 
                            Cursor_Info.hCursor,
                            0, 
                            0, 
                            0,
                            0,
                            DI_NORMAL | DI_DEFAULTSIZE);

            IDXGISurface1_ReleaseDC(this->DXGISurface, NULL);
        }

        // Copy image into CPU access texture
        ID3D11DeviceContext_CopyResource(this->deviceContext, (ID3D11Resource *)this->DestImage, (ID3D11Resource *)this->GDIImage);
    } else {
        // Copy image into CPU access texture
        ID3D11DeviceContext_CopyResource(this->deviceContext, (ID3D11Resource *)this->DestImage, (ID3D11Resource *)this->AcquiredDesktopImage);
    }


    // Copy from CPU access texture to bitmap buffer
    D3D11_MAPPED_SUBRESOURCE resource;
	UINT subresource = 0;
    hr = ID3D11DeviceContext_Map(this->deviceContext, (ID3D11Resource *)this->DestImage, subresource, D3D11_MAP_READ, 0, &resource);
    
    returnIfFailure(hr, errorNum - __LINE__, "Failed to copy texture to CPU accessable bitmap buffer");
    
    // BMP 32 bpp
    BITMAPINFO	BmpInfo;
    ZeroMemory(&BmpInfo, sizeof(BITMAPINFO));
    BmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    BmpInfo.bmiHeader.biBitCount = 32;
    BmpInfo.bmiHeader.biCompression = BI_RGB;
    BmpInfo.bmiHeader.biWidth = OutputDuplDesc.ModeDesc.Width;
    BmpInfo.bmiHeader.biHeight = OutputDuplDesc.ModeDesc.Height;
    BmpInfo.bmiHeader.biPlanes = 1;
    BmpInfo.bmiHeader.biSizeImage = OutputDuplDesc.ModeDesc.Width * OutputDuplDesc.ModeDesc.Height * 4;

    UINT BmpRowPitch = OutputDuplDesc.ModeDesc.Width * 4;
    uint8_t * texData = (uint8_t*)malloc(BmpInfo.bmiHeader.biSizeImage * sizeof(uint8_t));
    uint8_t * endOfTexData = texData + (BmpInfo.bmiHeader.biSizeImage * sizeof(uint8_t)) - BmpRowPitch;

    // Copy out of the pData buffer
    DEBUGLOGGER("Copying pData");
    for (size_t h = 0; h < OutputDuplDesc.ModeDesc.Height; ++h) {
        memcpy(endOfTexData - h * BmpRowPitch, resource.pData + h * resource.RowPitch, BmpRowPitch);
    }
    DEBUGLOGGER("Completed copy");

    // Save bitmap buffer into the file ScreenShot.bmp
    CHAR MyDocPath[MAX_PATH];

    hr = SHGetFolderPath(NULL, CSIDL_MYPICTURES, NULL, SHGFP_TYPE_CURRENT, MyDocPath);
    
    returnIfFailure(hr, errorNum - __LINE__, "Failed to get folder path");

    CHAR * FilePath = strcat(MyDocPath, fileName);
    if (DEBUG_ENABLED == 1) {
        printf("FILE=%s::LINE=%d::DATA=FilePath: %s\n", __FILE__, __LINE__, FilePath);
    }

    FILE * f;
    f = fopen(FilePath, "wb");

    if (f != NULL)
    {
        BITMAPFILEHEADER	bmpFileHeader;

        bmpFileHeader.bfReserved1 = 0;
        bmpFileHeader.bfReserved2 = 0;
        bmpFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + BmpInfo.bmiHeader.biSizeImage;
        bmpFileHeader.bfType = *(WORD*)"BM";
        bmpFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

        fwrite(&bmpFileHeader, sizeof(BITMAPFILEHEADER), 1, f);
        fwrite(&BmpInfo.bmiHeader, sizeof(BITMAPINFOHEADER), 1, f);
        fwrite(texData, BmpInfo.bmiHeader.biSizeImage, 1, f);

        fclose(f);

        if (SHOW_IMAGE_FLAG == 1) {
            ShellExecute(0, 0, FilePath, 0, 0, SW_SHOW);
        }
        
    } else {
        free(texData);
        returnIfFailure(hr, errorNum - __LINE__, "Failed to open file");
    }

    free(this);
    free(texData);

    if (DEBUG_ENABLED == 1){
        printf("\nPress Enter too continue... \n");
        fgets(tmpfileName, _MAX_FNAME, stdin);
    }

    return 0;
}
