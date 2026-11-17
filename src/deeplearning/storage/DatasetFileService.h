/** @file
 * @brief DatasetFileService 的公共声明。
 * @ingroup dl_storage
 */
#pragma once
#include <QStringList>
#include <QVector>
#include <functional>
#include "deeplearning/domain/AnnotationShape.h"

#include "deeplearning/domain/DatasetTypes.h"
/** @brief 批量结果，分别列出成功、跳过、失败、警告及可重试源文件。
 * @ingroup dl_storage
 */
struct DatasetTransferReport {
    QStringList succeeded;
    QStringList skipped;
    QStringList failures;
    QStringList warnings;
    QStringList failedSources;
    bool cancelled = false;
    bool ok() const { return failures.isEmpty() && !cancelled; }
    QString summary() const;
};

/** @brief 同步执行图片标签配对、恢复移动和暂存导出，不依赖数据集界面。
 * @ingroup dl_storage
 */
class DatasetFileService {
public:
    enum Format { Coco, Yolo, Mask, Classification };
    /** @brief 调用方提供同步授权回调；返回 false 时停止后续写提交。 */
    using Allowed = std::function<bool()>;
    static bool copyAtomic(const QString& source,const QString& target,QString& error);
    static DatasetTransferReport removeSamples(const QStringList& images,const QString& imageDir,
        const QString& labelDir,const QString& recoveryRoot,const Allowed& allowed);
    /** @brief 同基名图片标签作为一组导入；先用统一读取器校验语义，再暂存/备份/覆盖。
     * @details categories 为既有槽位，允许从有效标签发现新槽位；binary/offset 沿用当前编码契约。
     * 冲突可跳过或覆盖，提交失败尝试恢复整组。
     */
    static DatasetTransferReport importFiles(const QStringList& images,const QStringList& labels,
        const QString& imageDir,const QString& labelDir,bool overwrite,const Allowed& allowed,
        const QStringList& categories={},bool binary=false,int offset=1);
    /** @brief 分类图像按固定划分与 OK/NG 目录导入；先解码，冲突跳过，提交前复核授权。 */
    static DatasetTransferReport importClassificationFiles(const QStringList& images,const QString& root,
        const QString& phase,const QString& category,const Allowed& allowed);
    /**
     * @brief 向空目标暂存导出；indices 为空表示全部，否则只导出明确索引。
     * @details 返回值区分成功、失败、跳过和有损警告；不覆盖已有导出。
     * 调用方负责展示报告并重新确认授权；文件配对不承诺断电事务。
     */
    static DatasetTransferReport exportFiles(Format format,const QString& destination,
        const QVector<DatasetSample>& samples,const QList<int>& indices,const QString& labelsDir,
        const QStringList& categories,const QString& labelType,const QString& modelType,const Allowed& allowed);
    static bool readShapes(const QString& path,QSize size,const QStringList& categories,bool binary,
        QList<AnnotationShape>& shapes,QString& error);
    static bool yoloText(const QList<AnnotationShape>& shapes,QSize size,const QString& task,
        QByteArray& bytes,QStringList& warnings,QString& error);
};
