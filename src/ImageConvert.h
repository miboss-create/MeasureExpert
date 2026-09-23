#ifndef IMAGECONVERT_H
#define IMAGECONVERT_H
#include <QtCore/QDebug>
#include <QtGui/QImage>
#include "HalconCpp.h"
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/opencv.hpp"

using namespace HalconCpp;

/**
 * 图像格式互转工具（OpenCV / Qt / Halcon）。
 *
 * cvMat2QImage / QImage2cvMat：Qt 与 OpenCV 桥接；
 * QImage2HImage / HImage2QImage：Qt 与 Halcon 桥接（Halcon 侧必须 CopyImage，见 QImage2HImage 注释）。
 */

//将 OpenCV 的 cv::Mat 类型图像转换为 QImage 类型
//@param mat 待转换的图像，支持 CV_8UC1、CV_8UC3、CV_8UC4 三种OpenCV 的数据类型
//@param clone true 表示与 Mat 不共享内存，更改生成的 mat 不会影响原始图像，false 则会与 mat 共享内存
//@param rb_swap 只针对 CV_8UC3 格式，如果 true 则会调换 R 与 B RGB->BGR，如果共享内存的话原始图像也会发生变化
//@return 转换后的 QImage 图像
inline QImage cvMat2QImage(const cv::Mat& mat, bool clone = true, bool rb_swap = true)
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
inline cv::Mat QImage2cvMat(QImage &image, bool clone = true, bool rb_swap = true)
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
 *
 * 注意：GenImage1 / GenImageInterleaved 默认只引用外部指针。
 * 这里必须 CopyImage，否则 QImage 生命周期结束后 HImage 会变成野指针，
 * 后续 FindShapeModel 等读到垃圾数据，常表现为 QString MaxSize 断言崩溃。
 */
inline bool QImage2HImage(QImage &from, HalconCpp::HImage &to)
{
    if (from.isNull()) {
        return false;
    }

    const int width = from.width();
    const int height = from.height();
    if (width <= 0 || height <= 0) {
        return false;
    }

    const QImage::Format format = from.format();

    try {
        if (format == QImage::Format_RGB32
            || format == QImage::Format_ARGB32
            || format == QImage::Format_ARGB32_Premultiplied) {
            // 始终拷贝到 HALCON 自有三通道缓冲，避免引用 QImage 的 32bit 布局
            HalconCpp::HImage rImg, gImg, bImg;
            rImg.GenImageConst("byte", width, height);
            gImg.GenImageConst("byte", width, height);
            bImg.GenImageConst("byte", width, height);

            HalconCpp::HString type;
            Hlong w = 0, h = 0;
            unsigned char *R = reinterpret_cast<unsigned char *>(rImg.GetImagePointer1(&type, &w, &h));
            unsigned char *G = reinterpret_cast<unsigned char *>(gImg.GetImagePointer1(&type, &w, &h));
            unsigned char *B = reinterpret_cast<unsigned char *>(bImg.GetImagePointer1(&type, &w, &h));
            for (int row = 0; row < height; ++row) {
                const QRgb *line = reinterpret_cast<const QRgb *>(from.constScanLine(row));
                unsigned char *rLine = R + row * width;
                unsigned char *gLine = G + row * width;
                unsigned char *bLine = B + row * width;
                for (int col = 0; col < width; ++col) {
                    rLine[col] = static_cast<unsigned char>(qRed(line[col]));
                    gLine[col] = static_cast<unsigned char>(qGreen(line[col]));
                    bLine[col] = static_cast<unsigned char>(qBlue(line[col]));
                }
            }
            to = rImg.Compose3(gImg, bImg);
            return true;
        }

        if (format == QImage::Format_RGB888) {
            HalconCpp::HImage rImg, gImg, bImg;
            rImg.GenImageConst("byte", width, height);
            gImg.GenImageConst("byte", width, height);
            bImg.GenImageConst("byte", width, height);

            HalconCpp::HString type;
            Hlong w = 0, h = 0;
            unsigned char *R = reinterpret_cast<unsigned char *>(rImg.GetImagePointer1(&type, &w, &h));
            unsigned char *G = reinterpret_cast<unsigned char *>(gImg.GetImagePointer1(&type, &w, &h));
            unsigned char *B = reinterpret_cast<unsigned char *>(bImg.GetImagePointer1(&type, &w, &h));
            for (int row = 0; row < height; ++row) {
                const unsigned char *line = from.constScanLine(row);
                unsigned char *rLine = R + row * width;
                unsigned char *gLine = G + row * width;
                unsigned char *bLine = B + row * width;
                for (int col = 0; col < width; ++col) {
                    rLine[col] = line[col * 3 + 0];
                    gLine[col] = line[col * 3 + 1];
                    bLine[col] = line[col * 3 + 2];
                }
            }
            to = rImg.Compose3(gImg, bImg);
            return true;
        }

        if (format == QImage::Format_Grayscale8 || format == QImage::Format_Indexed8) {
            to.GenImageConst("byte", width, height);
            HalconCpp::HString type;
            Hlong w = 0, h = 0;
            unsigned char *dst = reinterpret_cast<unsigned char *>(to.GetImagePointer1(&type, &w, &h));
            for (int row = 0; row < height; ++row) {
                memcpy(dst + row * width, from.constScanLine(row), static_cast<size_t>(width));
            }
            return true;
        }
    } catch (const HalconCpp::HException &) {
        return false;
    }

    return false;
}

/**
 * @brief HImage2QImage 将 Halcon 的 HImage 转换为 Qt 的 QImage
 * @param from HImage ，暂时只支持 8bits 灰度图像和 8bits 的 3 通道彩色图像
 * @param to QImage ，这里 from 和 to 不共享内存。如果 to 的内存大小合适，那么就不用重新分配内存。所以可以加快速度。
 * @return  true 表示转换成功，false 表示转换失败
 */
inline bool HImage2QImage(HalconCpp::HImage &from, QImage &to)
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
