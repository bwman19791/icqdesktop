#include "stdafx.h"

#include "../../../../corelib/enumerations.h"

#include "../../../core_dispatcher.h"
#include "../../../gui_settings.h"
#include "../../../utils/log/log.h"
#include "../../../utils/utils.h"

#include "../FileSizeFormatter.h"

#include "IFileSharingBlockLayout.h"
#include "FileSharingUtils.h"

#include "FileSharingBlockBase.h"

UI_COMPLEX_MESSAGE_NS_BEGIN

FileSharingBlockBase::FileSharingBlockBase(
    ComplexMessageItem *_parent,
    const QString &_link,
    const core::file_sharing_content_type _type)
    : GenericBlock(
        _parent,
        _link,
        (MenuFlags)(_type == core::file_sharing_content_type::undefined || _type == core::file_sharing_content_type::ptt ?
            (MenuFlags::MenuFlagFileCopyable | MenuFlags::MenuFlagLinkCopyable )
            : (MenuFlags::MenuFlagFileCopyable | MenuFlags::MenuFlagLinkCopyable | MenuFlags::MenuFlagOpenInBrowser)),
        true)
    , Link_(_link)
    , Type_(_type)
    , Layout_(nullptr)
    , IsSelected_(false)
    , CopyFile_(false)
    , DownloadRequestId_(-1)
    , BytesTransferred_(-1)
    , FileSizeBytes_(-1)
    , FileMetaRequestId_(-1)
    , PreviewMetaRequestId_(-1)
    , CheckLocalCopyRequestId_(-1)
    , MaxPreviewWidth_(0)
{
    assert(!Link_.isEmpty());

    if (Link_.endsWith(QChar::Space) || Link_.endsWith(QChar::LineFeed))
        Link_.truncate(Link_.length() - 1);

    assert(Type_ > core::file_sharing_content_type::min);
    assert(Type_ < core::file_sharing_content_type::max);
}

FileSharingBlockBase::~FileSharingBlockBase()
{

}

void FileSharingBlockBase::clearSelection()
{
    if (!IsSelected_)
    {
        return;
    }

    IsSelected_ = false;

    update();
}

QString FileSharingBlockBase::formatRecentsText() const
{
    using namespace core;

    switch (getType())
    {
        case file_sharing_content_type::gif:
        case file_sharing_content_type::image:
        case file_sharing_content_type::snap_gif:
        case file_sharing_content_type::snap_image:
            return QT_TRANSLATE_NOOP("contact_list", "Photo");

        case file_sharing_content_type::snap_video:
        case file_sharing_content_type::video:
            return QT_TRANSLATE_NOOP("contact_list", "Video");

        case file_sharing_content_type::ptt:
            return QT_TRANSLATE_NOOP("contact_list", "Voice message");

        default:
            ;
    }

    return QT_TRANSLATE_NOOP("contact_list", "File");
}

IItemBlockLayout* FileSharingBlockBase::getBlockLayout() const
{
    assert(Layout_);
    return Layout_;
}

QString FileSharingBlockBase::getProgressText() const
{
    assert(FileSizeBytes_ >= -1);
    assert(BytesTransferred_ >= -1);

    const auto isInvalidProgressData = ((FileSizeBytes_ <= -1) || (BytesTransferred_ < -1));
    if (isInvalidProgressData)
    {
        return QString();
    }

    return HistoryControl::formatProgressText(FileSizeBytes_, BytesTransferred_);
}

QString FileSharingBlockBase::getSelectedText(bool isFullSelect) const
{
    if (IsSelected_)
    {
        return getSourceText();
    }

    return QString();
}

bool FileSharingBlockBase::hasRightStatusPadding() const
{
    return true;
}

void FileSharingBlockBase::initialize()
{
    connectSignals(true);

    initializeFileSharingBlock();
}

bool FileSharingBlockBase::isBubbleRequired() const
{
    return false;
}

bool FileSharingBlockBase::isSelected() const
{
    return IsSelected_;
}

void FileSharingBlockBase::setMaxPreviewWidth(int width)
{
    MaxPreviewWidth_ = width;
}

void FileSharingBlockBase::selectByPos(const QPoint& from, const QPoint& to, const BlockSelectionType /*selection*/)
{
    const QRect globalWidgetRect(
        mapToGlobal(rect().topLeft()),
        mapToGlobal(rect().bottomRight()));

    auto selectionArea(globalWidgetRect);
    selectionArea.setTop(from.y());
    selectionArea.setBottom(to.y());
    selectionArea = selectionArea.normalized();

    const auto selectionOverlap = globalWidgetRect.intersected(selectionArea);
    assert(selectionOverlap.height() >= 0);

    const auto widgetHeight = std::max(globalWidgetRect.height(), 1);
    const auto overlappedHeight = selectionOverlap.height();
    const auto overlapRatePercents = ((overlappedHeight * 100) / widgetHeight);
    assert(overlapRatePercents >= 0);

    const auto isSelected = (overlapRatePercents > 45);
    setSelected(isSelected);
}

const QString& FileSharingBlockBase::getFileLocalPath() const
{
    return FileLocalPath_;
}

const QString& FileSharingBlockBase::getFilename() const
{
    return Filename_;
}

IFileSharingBlockLayout* FileSharingBlockBase::getFileSharingLayout() const
{
    assert(Layout_);
    return Layout_;
}

QString FileSharingBlockBase::getFileSharingId() const
{
    return extractIdFromFileSharingUri(getLink());
}

int64_t FileSharingBlockBase::getFileSize() const
{
    assert(FileSizeBytes_ >= -1);

    return FileSizeBytes_;
}

const QString& FileSharingBlockBase::getLink() const
{
    assert(!Link_.isEmpty());

    return Link_;
}

core::file_sharing_content_type FileSharingBlockBase::getType() const
{
    assert(Type_ > core::file_sharing_content_type::min);
    assert(Type_ < core::file_sharing_content_type::max);

    return Type_;
}

const QString& FileSharingBlockBase::getDirectUri() const
{
    return directUri_;
}

bool FileSharingBlockBase::isFileDownloaded() const
{
    return !FileLocalPath_.isEmpty();
}

bool FileSharingBlockBase::isFileDownloading() const
{
    assert(DownloadRequestId_ >= -1);

    return (DownloadRequestId_ > 0);
}

bool FileSharingBlockBase::isGifImage() const
{
    return (getType() == core::file_sharing_content_type::gif);
}

bool FileSharingBlockBase::isImage() const
{
    return ((getType() == core::file_sharing_content_type::image) ||
            (getType() == core::file_sharing_content_type::snap_image));
}

bool FileSharingBlockBase::isPlainFile() const
{
    return !isPreviewable();
}

bool FileSharingBlockBase::isPreviewable() const
{
    return ((getType() == core::file_sharing_content_type::image) ||
            (getType() == core::file_sharing_content_type::gif) ||
            (getType() == core::file_sharing_content_type::video) ||
            isSnap());
}

bool FileSharingBlockBase::isSnap() const
{
    return is_snap_file_sharing_content_type(getType());
}

bool FileSharingBlockBase::isVideo() const
{
    return ((getType() == core::file_sharing_content_type::video) ||
            (getType() == core::file_sharing_content_type::snap_video));
}

void FileSharingBlockBase::onMenuCopyLink()
{
    QApplication::clipboard()->setText(getLink());
}

void FileSharingBlockBase::onMenuOpenInBrowser()
{
    QDesktopServices::openUrl(getLink());
}

void FileSharingBlockBase::onMenuCopyFile()
{
    CopyFile_ = true;

    if (isFileDownloading())
    {
        return;
    }

    QUrl urlParser(getLink());

    auto filename = urlParser.fileName();
    if (filename.isEmpty())
    {
        static const QRegularExpression re("[{}-]");

        filename = QUuid::createUuid().toString();
        filename.remove(re);
    }

    startDownloading(true);
}

void FileSharingBlockBase::onMenuSaveFileAs()
{
    if (Filename_.isEmpty())
    {
        assert(!"no filename");
        return;
    }

    QUrl urlParser(Filename_);

    QString dir;
    QString file;
    if (!Utils::saveAs(urlParser.fileName(), Out file, Out dir))
    {
        return;
    }

    assert(!dir.isEmpty());
    assert(!file.isEmpty());

    const auto addTrailingSlash = (!dir.endsWith('\\') && !dir.endsWith('/'));
    if (addTrailingSlash)
    {
        dir += "/";
    }

    dir += file;

    SaveAs_ = dir;

    if (isFileDownloading())
    {
        return;
    }

    startDownloading(true);
}

void FileSharingBlockBase::requestMetainfo(const bool isPreview)
{
    const auto fsFunction = (
        isPreview ?
            core::file_sharing_function::download_preview_metainfo :
            core::file_sharing_function::download_file_metainfo);

    const auto requestId = GetDispatcher()->downloadSharedFile(
        getSenderAimid(),
        getLink(),
        get_gui_settings()->get_value<QString>(settings_download_directory, Utils::DefaultDownloadsPath()),
        QString(),
        fsFunction);

    assert(requestId > 0);

    if (isPreview)
    {
        assert(PreviewMetaRequestId_ == -1);
        PreviewMetaRequestId_ = requestId;

        __TRACE(
            "prefetch",
            "initiated file sharing preview metadata downloading\n"
            "    contact=<" << getSenderAimid() << ">\n"
            "    fsid=<" << getFileSharingId() << ">\n"
            "    request_id=<" << requestId << ">");

        return;
    }

    assert(FileMetaRequestId_ == -1);
    FileMetaRequestId_ = requestId;
}

void FileSharingBlockBase::requestDirectUri(const QObject* _object, std::function<void(bool _res, const QString& _uri)> _callback)
{
    GetDispatcher()->requestFileDirectUri(getLink(), _object, _callback);
}

void FileSharingBlockBase::setBlockLayout(IFileSharingBlockLayout *_layout)
{
    assert(_layout);

    if (Layout_)
    {
        delete Layout_;
    }

    Layout_ = _layout;
}

void FileSharingBlockBase::setSelected(const bool isSelected)
{
    if (IsSelected_ == isSelected)
    {
        return;
    }

    IsSelected_ = isSelected;

    update();
}

void FileSharingBlockBase::startDownloading(const bool sendStats)
{
    assert(!isFileDownloading());

    if (sendStats)
    {
        GetDispatcher()->post_stats_to_core(core::stats::stats_event_names::filesharing_download_file);
    }

    assert(DownloadRequestId_ == -1);
    DownloadRequestId_ = GetDispatcher()->downloadSharedFile(
        getSenderAimid(),
        getLink(),
        Ui::get_gui_settings()->get_value<QString>(settings_download_directory, Utils::DefaultDownloadsPath()),
        QString(),
        core::file_sharing_function::download_file);

    onDownloadingStarted();
}

void FileSharingBlockBase::stopDownloading()
{
    assert(isFileDownloading());

    GetDispatcher()->abortSharedFileDownloading(DownloadRequestId_);

    DownloadRequestId_ = -1;

    BytesTransferred_ = -1;

    onDownloadingStopped();
}

void FileSharingBlockBase::checkExistingLocalCopy()
{
    const auto procId = Ui::GetDispatcher()->downloadSharedFile(
        getSenderAimid(),
        getLink(),
        Ui::get_gui_settings()->get_value<QString>(settings_download_directory, Utils::DefaultDownloadsPath()),
        QString(),
        core::file_sharing_function::check_local_copy_exists);

    assert(CheckLocalCopyRequestId_ == -1);
    CheckLocalCopyRequestId_ = procId;
}

void FileSharingBlockBase::connectSignals(const bool isConnected)
{
    if (isConnected)
    {
        QMetaObject::Connection connection;

        connection = QObject::connect(
            GetDispatcher(),
            &core_dispatcher::fileSharingFileDownloaded,
            this,
            &FileSharingBlockBase::onFileDownloaded,
            (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
        assert(connection);

        connection = QObject::connect(
            GetDispatcher(),
            &core_dispatcher::fileSharingFileDownloading,
            this,
            &FileSharingBlockBase::onFileDownloading,
            (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
        assert(connection);

        connection = QObject::connect(
            GetDispatcher(),
            &core_dispatcher::fileSharingError,
            this,
            &FileSharingBlockBase::onFileSharingError,
            (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
        assert(connection);

        connection = QObject::connect(
            GetDispatcher(),
            &core_dispatcher::fileSharingFileMetainfoDownloaded,
            this,
            &FileSharingBlockBase::onFileMetainfoDownloaded,
            (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
        assert(connection);

        if (isPlainFile())
        {
            connection = QObject::connect(
                GetDispatcher(),
                &core_dispatcher::fileSharingLocalCopyCheckCompleted,
                this,
                &FileSharingBlockBase::onLocalCopyChecked,
                (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
        }

        if (isPreviewable())
        {
            connection = QObject::connect(
                GetDispatcher(),
                &core_dispatcher::fileSharingPreviewMetainfoDownloaded,
                this,
                &FileSharingBlockBase::onPreviewMetainfoDownloadedSlot,
                (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
            assert(connection);
        }

        return;
    }

    QObject::disconnect(
        GetDispatcher(),
        &core_dispatcher::fileSharingFileDownloaded,
        this,
        &FileSharingBlockBase::onFileDownloaded);

    QObject::disconnect(
        GetDispatcher(),
        &core_dispatcher::fileSharingFileDownloading,
        this,
        &FileSharingBlockBase::onFileDownloading);

    QObject::disconnect(
        GetDispatcher(),
        &core_dispatcher::fileSharingError,
        this,
        &FileSharingBlockBase::onFileSharingError);

    QObject::disconnect(
        GetDispatcher(),
        &core_dispatcher::fileSharingFileMetainfoDownloaded,
        this,
        &FileSharingBlockBase::onFileMetainfoDownloaded);

    if (isPlainFile())
    {
        QObject::disconnect(
            GetDispatcher(),
            &core_dispatcher::fileSharingLocalCopyCheckCompleted,
            this,
            &FileSharingBlockBase::onLocalCopyChecked);
    }

    if (isPreviewable())
    {
        QObject::disconnect(
            GetDispatcher(),
            &core_dispatcher::fileSharingPreviewMetainfoDownloaded,
            this,
            &FileSharingBlockBase::onPreviewMetainfoDownloadedSlot);
    }
}

void FileSharingBlockBase::onFileDownloaded(qint64 seq, QString rawUri, QString localPath)
{
    if (seq != DownloadRequestId_)
    {
        return;
    }

    assert(!localPath.isEmpty());
    assert(DownloadRequestId_ != -1);

    DownloadRequestId_ = -1;

    BytesTransferred_ = -1;

    FileLocalPath_ = localPath;

    onDownloaded();

    if (!SaveAs_.isEmpty())
    {
        QFile::copy(getFileLocalPath(), SaveAs_);
        SaveAs_ = QString();

        return;
    }

    if (CopyFile_)
    {
        CopyFile_ = false;
        Utils::copyFileToClipboard(localPath);

        return;
    }

    onDownloadedAction();
}

void FileSharingBlockBase::onFileDownloading(qint64 seq, QString rawUri, qint64 bytesTransferred, qint64 bytesTotal)
{
    assert(bytesTotal > 0);

    if (seq != DownloadRequestId_)
    {
        return;
    }

    BytesTransferred_ = bytesTransferred;

    onDownloading(bytesTransferred, bytesTotal);
}

void FileSharingBlockBase::onFileMetainfoDownloaded(qint64 seq, QString filename, QString downloadUri, qint64 size)
{
    if (FileMetaRequestId_ != seq)
    {
        return;
    }

    FileMetaRequestId_ = -1;

    FileSizeBytes_ = size;

    assert(Filename_.isEmpty());
    Filename_ = filename;

    directUri_ = downloadUri;

    onMetainfoDownloaded();
}

void FileSharingBlockBase::onFileSharingError(qint64 seq, QString /*rawUri*/, qint32 /*errorCode*/)
{
    assert(seq > 0);

    const auto isDownloadRequestFailed = (DownloadRequestId_ == seq);
    if (isDownloadRequestFailed)
    {
        DownloadRequestId_ = -1;
    }

    const auto isFileMetainfoRequestFailed = (FileMetaRequestId_ == seq);
    const auto isPreviewMetainfoRequestFailed = (PreviewMetaRequestId_ == seq);
    if (isFileMetainfoRequestFailed || isPreviewMetainfoRequestFailed)
    {
        FileMetaRequestId_ = -1;
        PreviewMetaRequestId_ = -1;
    }

    onDownloadingFailed(seq);
}

void FileSharingBlockBase::onLocalCopyChecked(qint64 seq, bool success, QString localPath)
{
    if (seq != CheckLocalCopyRequestId_)
    {
        return;
    }

    assert(isPlainFile());

    if (!success)
    {
        onLocalCopyInfoReady(false);

        return;
    }

    FileLocalPath_ = localPath;

    onLocalCopyInfoReady(true);
}

void FileSharingBlockBase::onPreviewMetainfoDownloadedSlot(qint64 seq, QString miniPreviewUri, QString fullPreviewUri)
{
    if (seq != PreviewMetaRequestId_)
    {
        return;
    }

    assert(isPreviewable());
    assert(!miniPreviewUri.isEmpty());
    assert(!fullPreviewUri.isEmpty());

    PreviewMetaRequestId_ = -1;

    onPreviewMetainfoDownloaded(miniPreviewUri, fullPreviewUri);
}

UI_COMPLEX_MESSAGE_NS_END