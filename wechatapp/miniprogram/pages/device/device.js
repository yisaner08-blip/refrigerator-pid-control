// 设备连接界面，对应小程序的首页，负责通过 IP 地址或扫码方式连接 ESP32 设备，
// 连接成功后实时显示冷冻室温度、工作模式和运行时间等设备状态，
// 并通过 2 秒轮询机制持续刷新数据，同时将连接状态和 IP 写入全局数据供 control 和 chart 页面共享，
// 底部还维护了一条最多 50 条的操作日志用于记录连接/断开等事件。
// pages/device/device.js
Page({
  data: {
    isConnected: false,      // 是否已连接设备
    isConnecting: false,     // 是否正在连接中（防重复点击）
    deviceIP: '127.0.0.1:8180',  // Wokwi端口转发
    connectTime: '',// 连接成功的时间（显示用）
    deviceInfo: null,//设备状态信息
    logs: [], // 操作日志数组
    statusTimer: null// 轮询定时器ID（用于定时刷新状态）
  },

  onLoad() {// 触发时机：页面首次加载。只可执行一次
    this.addLog('页面加载完成');

    const app = getApp();
    if (app.globalData.deviceIP) {//判断全局里有没有已保存的设备 IP
      this.setData({//有 IP 的话，恢复到当前页面的状态
        deviceIP: app.globalData.deviceIP,//输入框显示之前连过的 IP
        isConnected: app.globalData.isConnected//连接状态同步为已连接
      });
      this.addLog(`已恢复连接: ${app.globalData.deviceIP}`);//写一条日志，告诉用户"我帮你自动恢复了之前的连接"
      this.startPolling();//启动定时轮询，每 2 秒调一次 /api/status 刷新设备状态
    }
  },

  onShow() {//触发时机：页面每次显示，可执行多次
    const app = getApp();//拿全局状态，判断是否已经连接上设备
    if (app.globalData.isConnected) {
      this.setData({//同步状态到当前页面
        isConnected: true,
        deviceIP: app.globalData.deviceIP
      });
      this.startPolling();//启动定时轮询
    }
  },

  onHide() {//触发时机：页面每次隐藏，可执行多次
    this.stopPolling();//停止定时轮询，不再发/api/status请求
  },

  onUnload() {//触发时机：页面销毁（彻底关闭，不再回来），只可执行一次
    this.stopPolling();//停止定时轮询，清理定时器
  },

  // IP地址输入
  onIPInput(e) {//e.detail.value 是输入框的值
    this.setData({
      deviceIP: e.detail.value//把输入框的当前值实时同步到页面数据
    });
  },

  // 连接设备
  async onConnect() {//点击"连接"按钮时触发，连接ESP32
    const ip = this.data.deviceIP;//从页面数据里取出用户输入的 IP
    
    if (!ip) {//IP 为空就弹提示，直接返回，不往下执行
      wx.showToast({
        title: '请输入IP地址',
        icon: 'none'
      });
      return;
    }

    this.setData({ isConnecting: true });//isConnecting = true：按钮禁用/显示"连接中"，防重复点击
    this.addLog(`正在连接设备: ${ip}`);//写一条日志

    try {
      // 测试连接
      const res = await this.requestAPI('/api/status');//调 /api/status，能拿到返回说明设备在线，连接成功

      const app = getApp();//保存设备IP到全局状态
      app.saveDeviceIP(ip);//本地缓存

      this.setData({//更新页面状态
        isConnected: true,
        isConnecting: false,
        deviceInfo: this.formatDeviceInfo(res),// 格式化温度数据
        connectTime: this.formatTime(new Date())// 记录连接时间
      });

      this.addLog('设备连接成功');
      this.startPolling();// 启动定时刷新状态
      
      wx.showToast({//显示连接成功提示
        title: '连接成功',
        icon: 'success'
      });
    } catch (error) {//catch 失败处理
      this.setData({
        isConnecting: false,// 解除"连接中"状态
        isConnected: false // 标记未连接
      });
      
      this.addLog(`连接失败: ${error}`);//写一条日志
      
      wx.showToast({//显示连接失败提示
        title: '连接失败',
        icon: 'none'
      });
    }
  },

  // 断开连接
  onDisconnect() {//断开连接按钮的回调函数
    this.stopPolling();//停止定时轮询，不再发 /api/status 请求
    const app = getApp();
    app.clearDeviceIP();
    /*调用 app.js 里的 clearDeviceIP()，做三件事：
    globalData.deviceIP = ''
    globalData.isConnected = false
    清除本地缓存 wx.removeStorageSync('deviceIP') 
    */
    
    this.setData({//更新当前页面状态
      isConnected: false,//标记未连接
      deviceInfo: null//清空设备信息
    });
    
    this.addLog('已断开设备连接');//写一条日志
    
    wx.showToast({//显示断开连接提示
      title: '已断开',
      icon: 'success'
    });
  },

  // 扫描二维码
  onScanQRCode() {//扫描二维码获取设备 IP 的函数
    wx.scanCode({//调用微信小程序的扫码 API，调起摄像头扫二维码
      success: (res) => {//扫码成功回调
        const ip = res.result;// 二维码内容（应该是设备 IP）
        this.setData({ deviceIP: ip });// 自动填入 IP 输入框，用户不用手动打字
        this.addLog(`扫描到设备IP: ${ip}`);
      },
      fail: () => {//扫码取消/失败回调
        this.addLog('扫描取消');//扫码取消/失败回调
      }
    });
  },

  // 请求API
  requestAPI(path) {//封装所有 HTTP 请求的底层函数，返回Promise
    return new Promise((resolve, reject) => {
      // ===== 开发模式：模拟数据（Wokwi仿真时用）=====
      const MOCK_MODE = false;  // false=使用Wokwi仿真，true=模拟数据
      
      if (MOCK_MODE) {//Mock 模式分支
        setTimeout(() => {
          if (path === '/api/status') {
            resolve({
              temp: -18.5,
              target: -18.0,
              mode: 'COOLING',
              compressor: true,
              pid_output: 45.2
            });
          } else if (path === '/api/pid') {
            resolve({ kp: 2.0, ki: 5.0, kd: 1.0 });
          } else {
            resolve({ success: true });
          }
        }, 300);
        return;
      }
      /*setTimeout 模拟网络延迟（300ms）
      根据 path 返回不同的假数据
      resolve 把数据传回调用方 
      */
      
      // ===== 真实设备模式 =====
      const ip = this.data.deviceIP;//取出当前页面存的 IP 地址
      const baseUrl = ip.includes('://') ? ip : `http://${ip}`;//拼完整 URL 前缀
      
      wx.request({
        url: `${baseUrl}${path}`,//拼完整 URL
        method: 'GET',//GET 请求（查状态用 GET，改参数用 POST）
        timeout: 5000,//5 秒超时，设备没响应就放弃，进入 fail 回调
        success: (res) => {//成功回调
          if (res.statusCode === 200) {
            resolve(res.data);// HTTP 200 → 成功，把数据传回去
          } else {
            reject(`HTTP ${res.statusCode}`);// 非200 → 失败
          }
        },
        fail: (err) => {//失败回调
          reject(err.errMsg || '网络错误'); // 超时、断网、DNS 失败等
        }
      });
    });
  },

  // 添加日志
  // 启动实时轮询
  startPolling() {//启动定时轮询，每 2 秒刷新一次设备状态
    if (this.data.statusTimer) return;//如果定时器已经存在，直接返回，防止重复启动
    this.refreshDeviceInfo();//先立刻查一次设备状态，不用等 2 秒。
    const timer = setInterval(() => this.refreshDeviceInfo(), 2000);//启动定时器，每 2000ms（2 秒） 调一次 refreshDeviceInfo()，拉最新的设备状态
    this.setData({ statusTimer: timer });//把定时器 ID 存进 data，方便后续 clearInterval(this.data.statusTimer) 停止
  },

  // 停止轮询
  stopPolling() {
    if (this.data.statusTimer) {//定时器存在才执行，防止报错
      clearInterval(this.data.statusTimer);//清除定时器，不再执行 refreshDeviceInfo
      this.setData({ statusTimer: null });//把定时器 ID 置空，标记"未轮询中"
    }
  },

  // 刷新设备信息
  async refreshDeviceInfo() {//定时轮询时调用的函数，拉取最新设备状态并更新页面
    try {//async + try/catch，发请求可能失败，要捕获异常
      const res = await this.requestAPI('/api/status');//调 /api/status，拿到设备当前状态（温度、模式、压缩机状态等）
      this.setData({ deviceInfo: this.formatDeviceInfo(res) });//把返回的数据格式化后更新到页面
    } catch (error) {
      // 静默失败，保留旧数据
      // 请求失败不弹错误、不刷日志，保留上次的数据继续显示
    }
  },

  // 格式化设备信息（温度保留1位小数）
  formatDeviceInfo(info) {//入参 info 是 /api/status 返回的 JSON 对象
    info.freezer_temp = Number(info.freezer_temp).toFixed(1);//Number()：转成数字。.toFixed(1)：保留 1 位小数，返回字符串
    info.freezer_setpoint = Number(info.freezer_setpoint).toFixed(1);//同样处理设定温度
    return info;//返回格式化后的对象
  },

  addLog(content) {//content 是日志内容
    const logs = this.data.logs;
    logs.unshift({
      time: this.formatTime(new Date()),//每条日志有 time（当前时间，格式 HH:MM:SS）和 content（内容）
      content: content
    });
    
    // 最多保留50条日志
    if (logs.length > 50) {
      logs.pop();//超过 50 条就删掉最后一条（最旧的），保持数组最多 50 条
    }
    
    this.setData({ logs });//更新页面数据，日志列表重新渲染
  },

  // 格式化时间，formatTime 把时间格式化成 HH:MM:SS，用于日志时间戳
  formatTime(date) {//入参 date 是一个 Date 对象
    const hour = date.getHours().toString().padStart(2, '0');
    const minute = date.getMinutes().toString().padStart(2, '0');
    const second = date.getSeconds().toString().padStart(2, '0');
    return `${hour}:${minute}:${second}`;//拼成 "21:13:05" 格式返回
  }
});
