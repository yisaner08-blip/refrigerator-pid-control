// app.js：
// 小程序全局入口，负责管理设备 IP 和连接状态，供所有页面共享。
App({
  onLaunch() {
    // 小程序启动时执行
    console.log('冰箱PID控制小程序已启动');
    
    // 从本地存储读取之前保存的IP地址
    const savedIP = wx.getStorageSync('deviceIP');
    if (savedIP) {
      this.globalData.deviceIP = savedIP;
    }
  },

  globalData: {
    deviceIP: '',  // ESP32 的 IP地址
    isConnected: false  // 连接状态
  },

  // 保存设备IP地址
  saveDeviceIP(ip) {//成功连接后调用
    this.globalData.deviceIP = ip;//把传入的 IP 地址存到全局变量 globalData.deviceIP 里
    this.globalData.isConnected = true;//标记连接状态为已连接
    wx.setStorageSync('deviceIP', ip);//把IP地址存到微信本地缓存
  },

  // 清除设备IP地址
  clearDeviceIP() {
    this.globalData.deviceIP = '';
    this.globalData.isConnected = false;
    wx.removeStorageSync('deviceIP');
  }
});
