// app.ts
App<IAppOption>({
  globalData: {
    deviceIP: '',
    userInfo: undefined
  },
  
  onLaunch() {
    console.log('小程序启动');
    
    // 从本地存储读取设备IP
    try {
      const deviceIP = wx.getStorageSync('deviceIP');
      if (deviceIP) {
        this.globalData.deviceIP = deviceIP;
        console.log('已加载设备IP:', deviceIP);
      }
    } catch (error) {
      console.error('读取设备IP失败:', error);
    }
  },
  
  onShow() {
    console.log('小程序显示');
  },
  
  onHide() {
    console.log('小程序隐藏');
  },
  
  // 保存设备IP
  saveDeviceIP(ip: string) {
    this.globalData.deviceIP = ip;
    try {
      wx.setStorageSync('deviceIP', ip);
    } catch (error) {
      console.error('保存设备IP失败:', error);
    }
  }
})
