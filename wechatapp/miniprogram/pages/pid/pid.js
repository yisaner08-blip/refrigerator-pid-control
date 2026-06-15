// pages/pid/pid.js
Page({
  data: {
    kp: 2.0,
    ki: 0.5,
    kd: 1.0,
    newKp: 2.0,
    newKi: 0.5,
    newKd: 1.0
  },

  onLoad() {
    this.loadCurrentPID();
  },

  // 加载当前PID参数
  async loadCurrentPID() {
    // TODO: 从设备获取当前PID参数
    // 暂时使用默认值
    this.setData({
      kp: 2.0,
      ki: 0.5,
      kd: 1.0,
      newKp: 2.0,
      newKi: 0.5,
      newKd: 1.0
    });
  },

  // Kp调整
  onKpChange(e) {
    this.setData({
      newKp: e.detail.value
    });
  },

  // Ki调整
  onKiChange(e) {
    this.setData({
      newKi: e.detail.value
    });
  },

  // Kd调整
  onKdChange(e) {
    this.setData({
      newKd: e.detail.value
    });
  },

  // 加载预设
  onLoadPreset(e) {
    const preset = e.currentTarget.dataset.preset;
    
    let kp, ki, kd;
    
    switch (preset) {
      case 'default':
        kp = 2.0;
        ki = 0.5;
        kd = 1.0;
        break;
      case 'aggressive':
        kp = 5.0;
        ki = 1.0;
        kd = 2.0;
        break;
      case 'conservative':
        kp = 1.0;
        ki = 0.2;
        kd = 0.5;
        break;
      default:
        return;
    }
    
    this.setData({
      newKp: kp,
      newKi: ki,
      newKd: kd
    });
    
    wx.showToast({
      title: '已加载预设',
      icon: 'success'
    });
  },

  // 应用参数
  async onApply() {
    const { newKp, newKi, newKd } = this.data;
    
    try {
      const app = getApp();
      const deviceIP = app.globalData.deviceIP;
      
      if (!deviceIP) {
        wx.showToast({
          title: '请先连接设备',
          icon: 'none'
        });
        return;
      }

      // 发送PID参数到设备
      await this.requestAPI(deviceIP, '/api/pid', 'POST', {
        kp: newKp,
        ki: newKi,
        kd: newKd
      });
      
      this.setData({
        kp: newKp,
        ki: newKi,
        kd: newKd
      });
      
      wx.showToast({
        title: '参数应用成功',
        icon: 'success'
      });
    } catch (error) {
      wx.showToast({
        title: '应用失败',
        icon: 'none'
      });
    }
  },

  // 重置
  onReset() {
    this.setData({
      newKp: this.data.kp,
      newKi: this.data.ki,
      newKd: this.data.kd
    });
    
    wx.showToast({
      title: '已重置',
      icon: 'success'
    });
  },

  // 请求API
  requestAPI(ip, path, method = 'GET', data) {
    return new Promise((resolve, reject) => {
      wx.request({
        url: `http://${ip}${path}`,
        method: method,
        data: data,
        timeout: 5000,
        success: (res) => {
          if (res.statusCode === 200) {
            resolve(res.data);
          } else {
            reject(`HTTP ${res.statusCode}`);
          }
        },
        fail: (err) => {
          reject(err.errMsg || '网络错误');
        }
      });
    });
  }
});
