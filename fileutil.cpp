#include "fileutil.h"
#include "stringutils.h"
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <openssl/md5.h>
#include "pathmanager.h"
#include "datadef.h"
#include "Config.h"
#include "fileparser_fdz.h"
#include "fileparser_svgx.h"
#include "Log.h"


#define THUMB_FILE_SUFFIX ".bmp"

using namespace std;

FileUtil::FileUtil()
{

}

string &FileUtil::separator()
{
    static string s("/");
    return s;
}

string FileUtil::combinePath(const string &str1, const string &str2)
{
    return str1 + separator() + str2;
}

string FileUtil::getFilePath(const string &filePath)
{
    std::string path = filePath.substr(0, filePath.find_last_of("/") + 1);
    return path;
}

string FileUtil::getFilename(const string &filePath)
{
    std::string fileName = filePath.substr(filePath.find_last_of("/") + 1);
    return fileName;
}

string FileUtil::getcompleteBaseName(const string &filePath)
{
    size_t lastSlash = filePath.find_last_of('/');
    size_t lastDot = filePath.find_last_of('.');
    if (lastSlash != std::string::npos && lastDot != std::string::npos && lastDot > lastSlash) {
        return filePath.substr(lastSlash + 1, lastDot - lastSlash - 1);
    }
    return "";
}

bool FileUtil::endWith(const string &file, const string endInfo)
{
    return StringUtils::endWith(StringUtils::upperCase(file), StringUtils::upperCase(endInfo));
}

bool FileUtil::_isDir(const string &path)
{
    struct stat buf;
    if(0 == ::stat(path.c_str(), &buf)){
        if(S_IFDIR & buf.st_mode){
            return true;
        }
    }
    return false;
}

bool FileUtil::_isFile(const string &file)
{
    struct stat buf;
    if(0 == ::stat(file.c_str(), &buf)){
        if(S_IFREG & buf.st_mode){
            return true;
        }
    }
    return false;
}

bool FileUtil::isDirExist(const string &path)
{
    return _isDir(path);
}

bool FileUtil::isFileExist(const string &file)
{
    return _isFile(file);
}

bool FileUtil::isUDiskFile(const string &file)
{
    string usbPath = SingletonPathMgr::instance()->usbPath();
    if (usbPath.empty()) {
        return false;
    }
    return StringUtils::startWith(file, usbPath);
}

bool FileUtil::mkdir(const string &path)
{
    int result = ::mkdir(path.c_str(), 0777);
    if(result == -1 && errno == EEXIST){
        return true;
    }
    return result == 0 ?  true : false;
}

bool FileUtil::rmdir(const string &path)
{
    int result = ::rmdir(path.c_str());
    return result == 0 ?  true : false;
}

bool FileUtil::removeFile(const string &file)
{
    if(_isFile(file)){
        ::remove(file.c_str());
        ::sync();
    }

    return true;
}

bool FileUtil::removePath(const string &path)
{
    DIR *dir;
    struct dirent *entry;
    char filepath[1024];
    strcpy(filepath, path.c_str());

    dir = ::opendir(filepath);
    if (dir == NULL) {
        return false;
    }

    while ((entry = ::readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(filepath, sizeof(filepath), "%s/%s", path.c_str(), entry->d_name);

        if (entry->d_type == DT_DIR) {
            removePath(string(filepath)); // 递归删除子目录
        } else {
            ::remove(filepath);
        }
    }

    ::closedir(dir);
    return rmdir(path);// 删除空目录
}

bool FileUtil::renameFile(const string &_old, const string &_new)
{
    if(isFileExist(_new)){
        removeFile(_new);
    }
    int ret = ::rename(_old.c_str(), _new.c_str());
    ::sync();
    return !ret;
}

bool FileUtil::deleteFiles(const string &path, const string &file_ext/*=""*/)
{
    DIR* dir_ptr = ::opendir(path.c_str());
    if (!dir_ptr) {
        LOG_ERROR("Error opening directory");
        return false;
    }

    struct dirent* entry_ptr;
    char file_path[PATH_MAX];

    while ((entry_ptr = readdir(dir_ptr))) {
        if ("" == file_ext){
            if (entry_ptr->d_type == DT_REG) {
                snprintf(file_path, PATH_MAX, "%s/%s", path.c_str(), entry_ptr->d_name);
                int ret = remove(file_path);

                if (ret != 0) {
                    LOG_ERROR("Error deleting file");
                }
            }
        }
        else{
            if (StringUtils::endWith(entry_ptr->d_name, file_ext)){
                int ret = remove(file_path);

                if (ret != 0) {
                    LOG_ERROR("Error deleting file");
                }
            }
        }
    }

    closedir(dir_ptr);
    ::sync();
    ::sync();
    return true;
}

bool FileUtil::copyFile(const string &source, const string &dest)
{
    if (isFileExist(dest)) {
        if (!removeFile(dest)) {
        }
    }
    LOG_CODE_LOCATION;
    int ret = 0;
    //cp
    int srcFD, distFD;
    int count;
    char buffer[1024] = {0};
    char *ptr;

    if(-1 == (srcFD = ::open(source.c_str(), O_RDONLY))){
        perror("open file1\n");
        return 2;
    }
    if(-1 == (distFD = ::open(dest.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR))){
        perror("open file2\n");
        return 3;
    }
    struct stat fileStat;
    fstat(srcFD, &fileStat);
    printf("copyFile %s size %u\n",source.c_str(), fileStat.st_size);

    while(count = read(srcFD, buffer, 1024)){
        if (-1 == count){
            perror("read");
            LOG_STRERROR;
            ret = 4;
            goto lbl;
        }
        ptr = buffer;
        int count2 = write(distFD, ptr, count);
        if (-1 == count2){
            perror("write");
            LOG_STRERROR;
            ret = 5;
            goto lbl;
        }
        memset(buffer, 0, 1024);
    }
    LOG_CODE_LOCATION;
lbl:
    close(srcFD);
    close(distFD);
    ::sync();
    return ret;
}

bool FileUtil::copyFileToPath(const string &source, const string &path)
{
    string filename = getFilename(source);
    string dest = combinePath(path, filename);
    return copyFile(source, dest);
}

void FileUtil::_splitPath(const char *file_path, char *file_dir, char *file_name, char *file_ext)
{
    const char* last_slash = strrchr(file_path, '/');
    if (last_slash != NULL) {
        strcpy(file_name, last_slash + 1);
        strncpy(file_dir, file_path, last_slash - file_path + 1);
        file_dir[last_slash - file_path + 1] = '\0';
    } else {
        // 没有路径分隔符，文件名就是整个字符串
        strcpy(file_name, file_path);
        file_dir[0] = '\0';
    }

    char* last_dot = strrchr(file_name, '.');
    if (last_dot != NULL) {
        strcpy(file_ext, last_dot + 1);
        *last_dot = '\0';
    } else {
        // 没有点号，没有扩展名
        file_ext[0] = '\0';
    }
}

bool FileUtil::checkAndRenameFile(const char *oldName, char *newName)
{
    if (access(oldName, F_OK) == 0) {
        char new_file_path[256];
        char file_dir[256];
        char file_name[256];
        char file_ext[256];

        _splitPath(oldName, file_dir, file_name, file_ext);
        snprintf(new_file_path, sizeof(new_file_path), "%s%s(1).%s", file_dir, file_name, file_ext);
        LOG_INFO("new_file_path: %s", new_file_path);
        ::strcpy(newName, new_file_path);
        return checkAndRenameFile(new_file_path, newName);
    } else {
        return 0;
    }
}

int FileUtil::_saveThumbImage(const string &filename, const std::vector<char> &bytes)
{
    if (FileUtil::isFileExist(filename)) {
        FileUtil::removeFile(filename);
    }
//    QPixmap image;
//    if (!image.loadFromData((const uchar*)bytes.data(), bytes.size())) {
//        LogSingleton::instance()->writeLog(LL_ERROR, "Fail To loadFromData");
//        return -1;
//    }
//    QBitmap mask = image.createMaskFromColor(Qt::black, Qt::MaskInColor);
//    image.setMask(mask);
//    if (!image.save(QString::fromStdString(path))) {
//        LogSingleton::instance()->writeLog(LL_ERROR, "Fail To Save Image (%s)", path.c_str());
//        return -1;
//    }
    //TODO!!! maskPix
    ofstream ostrm(filename, std::ios::binary);
    ostrm.write(bytes.data(), bytes.size());
    ::sync();
    return 0;
}

int FileUtil::pExtractAndSaveThumb(const string &file)
{
    if (!FileUtil::isFileExist(file)) {
        return -1;
    }

    std::vector<char> normalthumbData;
    std::vector<char> smallthumbData;

    if (StringUtils::endWith(file, FORMAT_SVGX)){
        FileParser_SVGX fp(file);
        if (fp.checkFileInfo()){
            return -1;
        }
        fp.getSmallThumbnail(smallthumbData);
        fp.getNormalThumbnail(normalthumbData);
    }else if (StringUtils::endWith(file, FORMAT_FDZ)){
        FileParser_FDZ fp(file);
        if (fp.checkFileInfo()){
            return -1;
        }
        fp.getSmallThumbnail(smallthumbData);
        fp.getNormalThumbnail(normalthumbData);
    }

    string path = FileUtil::pThumbPath(file);
    string fileName =  StringUtils::getcompleteBaseName(file);

    // Save Thumb 不同尺寸的触摸屏，使用大的缩略图作为Thumb（需要缩放）
    if (0 != _saveThumbImage(FileUtil::combinePath(path, fileName + "_thumb" + THUMB_FILE_SUFFIX), smallthumbData)) {
        return -1;
    }

    if (0 != _saveThumbImage(FileUtil::combinePath(path, fileName + THUMB_FILE_SUFFIX), normalthumbData)) {
        return -1;
    }
    return 0;
}

string FileUtil::pGetThumbImageFile(const string &file)
{
    if (!isFileExist(file)) {
        return string();
    }
    string imageFile = pThumbPath(file);
    imageFile = combinePath(imageFile, StringUtils::getcompleteBaseName(file) + THUMB_FILE_SUFFIX);
    if (isFileExist(imageFile) || 0 == pExtractAndSaveThumb(file)) {
        return imageFile;
    }
    return string();
}

string FileUtil::pThumbPath(const string &file)
{
    string path = "";
    if (!isFileExist(file)) {
        return path;
    }
    bool isUDisk = isUDiskFile(file);
    if(isUDisk){
        path = SingletonPathMgr::instance()->get(PATH_THUMB_UDISK);
    }else{
        if( StringUtils::startWith(file, SingletonPathMgr::instance()->get(PATH_MODEL_HISTORY))){
            path = SingletonPathMgr::instance()->get(PATH_THUMB_INTERNAL_HISTORY);
        }else{
            path = SingletonPathMgr::instance()->get(PATH_THUMB_INTERNAL);
        }
    }

    return path;
}

bool FileUtil::pCheckFileInfo(const string &file)
{
    if (endWith(file, FORMAT_SVGX)){
        FileParser_SVGX fp(file);
        return fp.checkFileInfo();
    }else if (endWith(file, FORMAT_FDZ)){
        FileParser_FDZ fp(file);
        return fp.checkFileInfo();
    }
    return false;
}

bool FileUtil::pRemoveFile(const string &file)
{
    if (!isFileExist(file)) {
        return false;
    }

    removeFile(pGetThumbImageFile(file));
    bool ret = removeFile(file);
    ::sync();
    return ret;
}

string FileUtil::pGetModelSliceInfo(const string &file)
{
    if (!FileUtil::isFileExist(file)) {
        return std::string();
    }
    PrintChunkList list;
    if (endWith(file, FORMAT_SVGX)){
        FileParser_SVGX fp(file);
        if (fp.getPrintChunkList(list)){
            return std::string();
        }
    }else if (endWith(file, FORMAT_FDZ)){
        FileParser_FDZ fp(file);
        if (fp.getPrintChunkList(list)){
            return std::string();
        }
    }
    std::string info;
    char tmp[100];
    // 0-100|50um|3.5s;100-200|25um|5s;
    for (size_t i = 0; i < list.size(); i++){
        const PrintChunk& chunk = list[i];
        snprintf(tmp, 100, "%i-%i|%ium|%.1fs;", chunk.startLayerPos, chunk.endLayerPos,
                            int(chunk.layerHeight * 1000), chunk.layerPara.expose.modelTime / 1000.0f);
        info.append(tmp);
    }
    return info;
}

int FileUtil::getDirs(const string &path,  std::vector<string> &outDirs){
    char dirPath[1024];

    DIR* dir = ::opendir(path.c_str());
    if (dir == nullptr) {
        LOG_CODE_LOCATION;
        return -1;
    }

    struct dirent* entry;
    while ((entry = ::readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (entry->d_type == DT_DIR) {
            snprintf(dirPath, sizeof(dirPath), "%s/%s", path.c_str(), entry->d_name);
            outDirs.push_back(string(dirPath));
        }
    }

    closedir(dir);
    return 0;
}

int FileUtil::getFiles(const string &path, const string endInfo, std::vector<string> &outfiles)
{
    DIR* dir = ::opendir(path.c_str());
    if (dir == nullptr) {
        LOG_CODE_LOCATION;
        return -1;
    }

    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        std::string fileName = entry->d_name;
        if(endWith(fileName, endInfo)){
            outfiles.push_back(combinePath(path, fileName));
        }
    }
    closedir(dir);

    return 0;
}

string FileUtil::_getFile(const string &path, const string endInfo)
{
    vector<string> fileList;

    DIR* dir = ::opendir(path.c_str());
    if (dir == nullptr) {
        LOG_CODE_LOCATION;
        return "";
    }

    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        std::string fileName = entry->d_name;
        if(endWith(fileName, endInfo)){
            fileList.push_back(fileName);
            break;
        }
    }
    closedir(dir);

    if (fileList.empty()) {
        return "";
    }
//    std::sort(fileList.begin(), fileList.end());
    return combinePath(path, fileList[0]);

}

string FileUtil::getCorrentFile(const string &path)
{
    const string endInfo(".corbin");
    return _getFile(path, endInfo);
}

string FileUtil::getMaskImage(const string &path)
{
    string serialNo = SingletonConfig::instance()->serialValue().machineSelrial;
    if(serialNo.empty()){
        serialNo = "serial";
    }
    const string filename = string("mask-") + serialNo + string(".png");
    string res = combinePath(path, filename);
    return res;
}

string FileUtil::getLicenceFile(const string &path)
{
    const string endInfo(".lic");
    return _getFile(path, endInfo);
}

string FileUtil::toHex(uint8_t *buf, uint16_t size)
{
    std::stringstream hexSS;
    for (int i = 0; i < size; i++){
        hexSS << std::setw(2) << std::setfill('0') << std::hex << (int)buf[i];
    }
    std::string hexStr = hexSS.str();
    return hexStr;
}

void FileUtil::getFileMD5Value(const string filename, string &md5Value)
{
    md5Value.clear();

    MD5_CTX ctx;
    unsigned char tempMd5Value[16];
    char buffer[1024];
    int len=0;
    FILE * fp=NULL;
    memset(tempMd5Value,0,sizeof(tempMd5Value));
    memset(buffer,0,sizeof(buffer));

    fp = fopen(filename.c_str(),"rb");
    if(fp == NULL){
        printf("%s,Can't open file\n", __func__);
        return;
    }

    ::MD5_Init(&ctx);
    while((len=fread(buffer,1,1024,fp))>0)
    {
        ::MD5_Update(&ctx,buffer,len);
        memset(buffer,0,sizeof(buffer));
    }
    ::MD5_Final(tempMd5Value,&ctx);

    fclose(fp);

    md5Value = toHex(tempMd5Value, sizeof(tempMd5Value));
    //    std::cout << md5Value << std::endl;
}

int64_t FileUtil::getFileSize(const string filename)
{
    int64_t filesize = -1;
    struct stat statbuff;
    if(stat(filename.c_str(), &statbuff) < 0){
        return filesize;
    }else{
        filesize = statbuff.st_size;
    }
    return filesize;
}

