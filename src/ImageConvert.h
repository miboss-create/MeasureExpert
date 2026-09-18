#ifndef IMAGECONVERT_H
#define IMAGECONVERT_H
#include <QtCore/QDebug>
#include <QtGui/QImage>
#include "HalconCpp.h"
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/opencv.hpp"

using namespace HalconCpp;
/**************************************************
 *cv::Mat 与 QImage 之间的转换函数。在 Qt 程序中使用 OpenCV 时有用。
 *
 **************************************************/

//将 OpenCV 的 cv::Mat 类型图像转换为 QImage 类型
//@param mat 待转换的图像，支持 CV_8UC1、CV_8UC3、CV_8UC4 三种OpenCV 的数据类型
//@param clone true 表示与 Mat 不共享内存，更改生成的 mat 不会影响原始图像，false 则会与 mat 共享内存
//@param rb_swap 只针对 CV_8UC3 格式，如果 true 则会调换 R 与 B RGB->BGR，如果共享内存的话原始图像也会发生变化
//@return 转换后的 QImage 图像
QImage cvMat2QImage(const cv::Mat& mat, bool clone = true, bool rb_swap = true)
{
    const uchar *pSrc = (const uchar*)mat.data;
    // 8-bits unsigned, NO. OF CHANNELS = 1
    if(mat.type() == CV_8UC1)
    {
        //QImage image(mat.cols, mat.rows, QImage::Format_Grayscale8);
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        if(clone) return image.copy();
        return image;
    }
    // 8-bits unsigned, NO. OF CHANNELS = 3
    else if(mat.type() == CV_8UC3)
    {
        // Create QImage with same dimensions as input Mat
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        if(clone)
        {
            if(rb_swap) return image.rgbSwapped();
            return image.copy();
        }
        else
        {
            if(rb_swap)
            {
                cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
            }
            return image;
        }

    }
    else if(mat.type() == CV_8UC4)
    {
        qDebug() << "CV_8UC4";
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        if(clone) return image.copy();
        return image;
    }
    else
    {
        qDebug() << "ERROR: Mat could not be converted to QImage.";
        return QImage();
    }
}

//@brief 将 QImage 的类型图像转换为 cv::Mat 类型
//@param image 待转换的图像，支持 Format_Indexed8/Format_Grayscale、24 位彩色、32 位彩色格式，
//@param clone true 表示与 QImage 不共享内存，更改生成的 mat 不会影响原始图像，false 则会与 QImage 共享内存
//@param rg_swap 只针对 RGB888 格式，如果 true 则会调换 R 与 B RGB->BGR，如果共享内存的话原始图像也会发生变化
//@return 转换后的 cv::Mat 图像
cv::Mat QImage2cvMat(QImage &image, bool clone = true, bool rb_swap = true)
{
    cv::Mat mat;
    qDebug() << image.format();
    switch(image.format())
    {
    case QImage::Format_ARGB32:
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32_Premultiplied:
        mat = cv::Mat(image.height(), image.width(), CV_8UC4, (void *)image.constBits(), image.bytesPerLine());
        if(clone)  mat = mat.clone();
        break;
    case QImage::Format_RGB888:
        mat = cv::Mat(image.height(), image.width(), CV_8UC3, (void *)image.constBits(), image.bytesPerLine());
        if(clone)  mat = mat.clone();
        if(rb_swap) cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
        break;
    case QImage::Format_Indexed8:
    case QImage::Format_Grayscale8:
        mat = cv::Mat(image.height(), image.width(), CV_8UC1, (void *)image.bits(), image.bytesPerLine());
        if(clone)  mat = mat.clone();
        break;
    }
    return mat;
}

/**
 * @brief QImage2HImage 将 Qt QImage 转换为 Halcon 的 HImage
 * @param from 输入的 QImage
 * @param to 输出的 HImage ，from 和 to 不共享内存数据。 每次都会为 to 重新分配内存。
 * @return true 表示转换成功，false 表示转换失败。
 */
bool QImage2HImage(QImage &from, HalconCpp::HImage &to)
{
    if(from.isNull()) return false;

    int width = from.width(), height = from.height();
    QImage::Format format = from.format();

    if(format == QImage::Format_RGB32 ||
        format == QImage::Format_ARGB32 ||
        format == QImage::Format_ARGB32_Premultiplied)
    {
        if(from.bytesPerLine() == 4 * width)
        {
            to.GenImageInterleaved(from.bits(), "bgrx", width, height, 0,  "byte", width, height, 0, 0, 8, 0);
        }
        else
        {
            to.GenImageInterleaved(from.bits(), "bgrx", width, height, 0,  "byte", width, height, 0, 0, 8, 0);
            uchar *R, *G, *B;
            HString Type;
            Hlong Width, Height;
            to.GetImagePointer3(reinterpret_cast<void **>(&R),
                                reinterpret_cast<void **>(&G),
                                reinterpret_cast<void **>(&B), &Type, &Width, &Height);
            for(int row = 0; row < height; row ++)
            {
                QRgb* line = reinterpret_cast<QRgb*>(from.scanLine(row));
                for(int col = 0; col < width; col ++)
                {
                    *R = qRed(line[col]);
                    *G = qGreen(line[col]);
                    *B = qBlue(line[col]);
                    ++R;
                    ++G;
                    ++B;
                }
            }
        }
        return true;
    }
    else if(format == QImage::Format_RGB888)
    {
        if(from.bytesPerLine() == 3 * width)
        {
            to.GenImageInterleaved(from.bits(), "rgb", width, height, 0,  "byte", width, height, 0, 0, 8, 0);
        }
        else
        {
            to.GenImageInterleaved(from.bits(), "rgb", width, height, 0,  "byte", width, height, 0, 0, 8, 0);
            uchar *R, *G, *B;
            HString Type;
            Hlong Width, Height;
            to.GetImagePointer3(reinterpret_cast<void **>(&R),
                                reinterpret_cast<void **>(&G),
                                reinterpret_cast<void **>(&B), &Type, &Width, &Height);
            for(int row = 0; row < height; row ++)
            {
                unsigned char* line = reinterpret_cast<unsigned char *>(from.scanLine(row));
                for(int col = 0; col < width; col ++)
                {
                    *R ++ = *line ++;
                    *G ++ = *line ++;
                    *B ++ = *line ++;
                }
            }
        }
        return true;
    }
    else if(format == QImage::Format_Grayscale8 || format == QImage::Format_Indexed8)
    {
        if(from.bytesPerLine() == width)
        {
            to.GenImage1("byte", width, height, from.bits());
        }
        else// 这时说明每行数据之间有填充字节。因此需要重新写数据
        {
            to.GenImageConst("byte", width, height);
            Hlong W, H; HString Type;
            unsigned char * pTo = reinterpret_cast<unsigned char *>( to.GetImagePointer1(&Type, &W, &H) );
            for(int row = 1; row < H; row ++)
            {
                const unsigned char * pSrc = from.constScanLine(row);
                unsigned char * pDist = pTo + row * W;
                memcpy( pDist, pSrc, static_cast<size_t>(W));
            }
        }
        return true;
    }
    return false;
}

/**
 * @brief HImage2QImage 将 Halcon 的 HImage 转换为 Qt 的 QImage
 * @param from HImage ，暂时只支持 8bits 灰度图像和 8bits 的 3 通道彩色图像
 * @param to QImage ，这里 from 和 to 不共享内存。如果 to 的内存大小合适，那么就不用重新分配内存。所以可以加快速度。
 * @return  true 表示转换成功，false 表示转换失败
 */
bool HImage2QImage(HalconCpp::HImage &from, QImage &to)
{
    Hlong width;
    Hlong height;
    from.GetImageSize(&width, &height);

    HTuple channels = from.CountChannels();
    HTuple type = from.GetImageType();

    if( strcmp(type[0].S(), "byte" )) // 如果不是 byte 类型，则失败
    {
        return false;
    }

    QImage::Format format;
    switch(channels[0].I())
    {
    case 1:
        format = QImage::Format_Grayscale8;
        break;
    case 3:
        format = QImage::Format_RGB888;
        break;
    default:
        return false;
    }

    if(to.width() != width || to.height() != height || to.format() != format)
    {
        to = QImage(static_cast<int>(width),
                    static_cast<int>(height),
                    format);
    }
    HString Type;
    if(channels[0].I() == 1)
    {
        unsigned char * pSrc = reinterpret_cast<unsigned char *>( from.GetImagePointer1(&Type, &width, &height) );
        if(to.bytesPerLine() == width)
        {
            memcpy( to.bits(), pSrc, static_cast<size_t>(width) * static_cast<size_t>(height) );
        }
        else
        {
            for(int row = 1; row < height; row ++)
            {
                unsigned char * pDistLine = to.scanLine(row);
                const unsigned char * pSrcLine = pSrc + row * width;
                memcpy( pDistLine, pSrcLine, static_cast<size_t>(width));
            }
        }

        return true;
    }
    else if(channels[0].I() == 3)
    {
        uchar *R, *G, *B;
        from.GetImagePointer3(reinterpret_cast<void **>(&R),
                              reinterpret_cast<void **>(&G),
                              reinterpret_cast<void **>(&B), &Type, &width, &height);

        for(int row = 0; row < height; row ++)
        {
            unsigned char * line = reinterpret_cast<unsigned char *>(to.scanLine(row));
            for(int col = 0; col < width; col ++)
            {
                *line++ = *R++;
                *line++ = *G++;
                *line++ = *B++;
            }
        }
        return true;
    }

    return false;
}


#endif // IMAGECONVERT_H
