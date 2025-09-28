设计方案：先设置默认值，然后读取配置文件，再同步保存配置文件
原因：如果配置文件本来就存在，则不会有什么影响，如果配置文件不存在，则使用默认值生成了一份配置文件

// 读取配置文件
int Config::loadPrinterConfig(const std::string &confFile)
{
    ifstream is(confFile, std::ios::binary);
    Json::Reader    reader;
    Json::Value     root;
    if( is.is_open() && reader.parse(is, root)) {
		// 参数2：默认值，如果键存在则返回读取到的值，否则返回默认值，使用 .asFloat() 来显式地将属性值转换为浮点数类型
        m_pC.Width              = root.get("Width", m_pC.Width).asFloat(); 
        m_pC.Height             = root.get("Height", m_pC.Height).asFloat();
        m_pC.XRes               = root.get("XRes", m_pC.XRes).asUInt();
        m_pC.YRes               = root.get("YRes", m_pC.YRes).asUInt();
		
		Json::Value ZMotor = root["ZMotor"];
        m_pC.Pitch                  = ZMotor.get("Pitch", m_pC.Pitch).asUInt();
	}
}

// 修改配置文件
void Config::syncPrinterConfig()
{
    Json::Value     root;
    root.clear();

    root["Width"]           = m_pC.Width;
    root["LightIntensity"]  = m_pC.LightIntensity;
    root["LightCurrent"]    = m_pC.LightCurrent;
    root["OutlineGrayOffset"]            = m_pC.OutlineGrayOffset;
    Json::Value ZMotor;
        ZMotor.clear();
        ZMotor["Pitch"]           = m_pC.Pitch;
        Json::Value MotorParaValue;
            MotorParaValue.clear();
            MotorParaValue["A"]    = m_pC.MotorConfig.mmcI.A;
            MotorParaValue["D"]    = m_pC.MotorConfig.mmcI.D;
        ZMotor["MotorParaI"]  = MotorParaValue;
            MotorParaValue["A"]    = m_pC.MotorConfig.mmcII.A;
            MotorParaValue["D"]    = m_pC.MotorConfig.mmcII.D;
        ZMotor["MotorParaII"]       = MotorParaValue;
        root["ZMotor"]              = ZMotor;

    Json::Value LimitValue;
        LimitValue.clear();
        LimitValue["PrintLayerLimit"] = m_pC.PrintLayerLimit;
        root["LimitValue"] = LimitValue;

    root.setComment(string("/* Printer Config */"), Json::commentAfter);
    int fd = open(m_printerCfgFile.c_str(), O_WRONLY | O_CREAT | O_SYNC /*| O_DIRECT*/, 00664);
    if (fd == -1){
        perror("**open ");
        return;
    }

    Json::StyledWriter  fwriter;
    std::string data = fwriter.write(root);
    int count = write(fd, (void*)data.c_str(), data.size());
    if (count == -1){
        perror("**write ");
        return;
    }
    fsync(fd);
    close(fd);
    systemSync();
    if(count != data.size()){
        printf("write data size[%d], json size[%d]", count, data.size());
        return;
    }
    return;
}