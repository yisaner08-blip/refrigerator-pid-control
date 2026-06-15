// 对应的是控制界面，作为用户对冰箱发送控制指令的操作台，
// 负责切换工作模式（制冷/制热/除霜等）、调节温度设定点（带 -30~-10°C 安全范围校验）、触发快速操作（强制除霜、刷新状态），
// 并通过 requestAPI 封装的 HTTP 请求与 ESP32 通信，
// 所有控制指令通过 POST 请求发送到 /api/mode 和 /api/setpoint 接口，
// 同时支持 Mock 模式离线调试
// pages/control/control.js
Page({
  data: {
    freezerTemp: '-18.5',       // 当前温度（显示用，字符串）
    freezerSetpoint: '-18.0',   // 设定温度（显示用，字符串）
    currentMode: 0,              // 当前模式：0=制冷 1=除霜 2=节能 3=故障
    compressorOn: false,         // 压缩机状态
    fanOn: false,                // 风扇状态
    defrostOn: false,            // 除霜状态
    statusTimer: null             // 轮询定时器ID
  },

  onLoad() {
    this.startPolling(); // 启动定时刷新状态
  },

  onShow() {
    this.startPolling(); // 切Tab回来时恢复轮询
  },

  onHide() {
    this.stopPolling();   // 切到其他Tab时停止轮询
  },

  onUnload() {
    this.stopPolling(); // 页面关闭时清理定时器
  },

  startPolling() { //启动轮询
    if (this.data.statusTimer) return; // 已有定时器则不重复启动
    this.refreshStatus(); // 先立刻查一次
    const timer = setInterval(() => this.refreshStatus(), 2000);  // 每2秒刷新
    this.setData({ statusTimer: timer });  // 存定时器ID
  },

  stopPolling() {
    if (this.data.statusTimer) {// 定时器存在才执行
      clearInterval(this.data.statusTimer);  // 清除定时器
      this.setData({ statusTimer: null });   // 置空，标记未轮询
    }
  },

  // ==================== 状态刷新函数 ====================
  // 刷新状态
  async refreshStatus() {
    try {
      // 获取小程序全局实例，用于访问全局数据
      const app = getApp();
      // 从全局数据中获取设备IP地址，这是连接硬件设备的关键信息
      const deviceIP = app.globalData.deviceIP;// 从全局拿设备IP
      
      // 安全检查：如果设备IP不存在，说明用户还未连接设备
      if (!deviceIP) {
        // 显示提示信息，引导用户先连接设备
        wx.showToast({
          title: '请先连接设备',  // 提示文字
          icon: 'none'           // 不显示图标，纯文字提示
        });
        return;  // 终止函数执行，不再继续请求
      }

      // 调用封装的API请求函数，获取设备状态
      // '/api/status' 是后端提供的状态查询接口
      // await 等待异步请求完成，确保拿到数据后再执行后续代码
      const res = await this.requestAPI('/api/status');
      
      // 将获取到的设备状态数据更新到页面数据中
      // setData 是微信小程序更新页面数据的核心方法，会触发界面重新渲染
      this.setData({
        // 将冷冻室当前温度转换为数字，并保留1位小数（如：-18.5）
        freezerTemp: Number(res.freezer_temp).toFixed(1),
        // 将冷冻室设定温度转换为数字，并保留1位小数
        freezerSetpoint: Number(res.freezer_setpoint).toFixed(1),
        // 设置当前工作模式（制冷/除霜/节能等），mode_code 是数字编码
        currentMode: res.mode_code,
        // 更新压缩机状态：true=开启，false=关闭
        compressorOn: res.compressor,
        // 更新风扇状态：0=关闭，1=开启
        fanOn: res.fan,
        // 更新除霜状态：'OFF'=关闭，'ON'=开启
        defrostOn: res.defrost
      });
    } catch (error) {
      // 如果请求失败（网络错误、设备离线等），捕获异常并记录
      console.error('刷新状态失败:', error);
      // 这里可以添加用户提示，比如显示"设备连接失败"
    }
  },

  // ==================== 模式设置函数 ====================
  // 设置模式
  async onSetMode(e) {
    // 从点击事件中获取用户选择的模式代码
    // e.currentTarget.dataset.mode 是WXML中 data-mode="X" 的值
    const mode = parseInt(e.currentTarget.dataset.mode);
    
    try {
      // 调用API接口，向设备发送模式切换指令
      await this.requestAPI('/api/mode', {
        method: 'POST',        // 使用POST方法，因为要修改设备状态
        data: { mode: mode }   // 发送模式代码给后端
      });
      
      // 请求成功后，立即更新本地页面状态，提升用户体验
      // 避免等待下次刷新才能看到状态变化
      this.setData({ currentMode: mode });
      
      // 显示成功提示
      wx.showToast({
        title: '模式设置成功',
        icon: 'success'  // 显示成功图标（绿色对勾）
      });
    } catch (error) {
      // 如果设置失败，显示错误提示
      wx.showToast({
        title: '设置失败',
        icon: 'none'
      });
    }
  },

  // ==================== 温度调整函数 ====================
  // 调整温度
  async onAdjustTemp(e) {
    // 从点击事件中获取温度调整值（如：+1 或 -1）
    const delta = parseInt(e.currentTarget.dataset.delta);
    // 计算新的设定温度 = 当前设定温度 + 调整值
    // parseFloat 确保是浮点数计算，避免整数除法问题
    const newSetpoint = parseFloat(this.data.freezerSetpoint) + delta;

    // 温度范围限制：-30°C 到 -10°C
    // 这是硬件安全限制，防止用户设置过低/过高温度
    if (newSetpoint < -30 || newSetpoint > -10) {
      wx.showToast({
        title: '温度范围: -30~-10°C',  // 提示用户合法的温度范围
        icon: 'none'
      });
      return;  // 终止函数，不发送非法温度给设备
    }

    try {
      // 获取设备IP，准备发送请求
      const app = getApp();
      const deviceIP = app.globalData.deviceIP;

      // 调用API接口，发送温度设定值给设备
      await this.requestAPI('/api/setpoint', {
        method: 'POST',  // POST方法用于修改设备参数
        data: {
          zone: 'freezer',   // 指定调节的区域（冷冻室）
          value: newSetpoint // 新的设定温度
        }
      });

      // 本地立即更新显示，提升响应速度
      this.setData({ freezerSetpoint: newSetpoint.toFixed(1) });
      
      // 显示成功提示
      wx.showToast({
        title: '温度设置成功',
        icon: 'success'
      });
    } catch (error) {
      // 设置失败时显示错误提示
      wx.showToast({
        title: '设置失败',
        icon: 'none'
      });
    }
  },

  // ==================== 快捷操作函数 ====================
  // 快捷操作
  async onQuickAction(e) {
    // 获取用户点击的快捷操作类型（如：'defrost' 或 'refresh'）
    const action = e.currentTarget.dataset.action;
    
    // 判断操作类型：除霜操作
    if (action === 'defrost') {
      try {
        // 发送除霜模式切换指令（模式1=除霜模式）
        await this.requestAPI('/api/mode', {
          method: 'POST',
          data: { mode: 1 }  // 1 = 除霜模式
        });
        
        // 更新本地状态显示
        this.setData({ currentMode: 1 });
        
        // 显示成功提示
        wx.showToast({
          title: '已启动除霜',
          icon: 'success'
        });
      } catch (error) {
        // 操作失败时显示错误提示
        wx.showToast({
          title: '操作失败',
          icon: 'none'
        });
      }
    } else if (action === 'refresh') {
      // 判断操作类型：刷新状态
      // 直接调用刷新状态函数，重新获取设备数据
      this.refreshStatus();
    }
  },

  // ==================== API请求封装函数 ====================
  // 请求API（与device.js保持一致）
  requestAPI(path, options = {}) {
    // 返回Promise对象，支持async/await语法
    return new Promise((resolve, reject) => {
      // 模拟数据模式开关：true=使用本地模拟数据，false=连接真实设备
      const MOCK_MODE = false;  // false=使用Wokwi仿真，true=模拟数据
      // 获取请求方法，默认是GET
      const method = options.method || 'GET';
      // 获取请求数据，用于POST/PUT等方法
      const data = options.data || null;
      
      // 如果处于模拟模式，直接返回模拟数据，不发送网络请求
      if (MOCK_MODE) {
        // 模拟网络延迟（300毫秒），让加载效果更真实
        setTimeout(() => {
          // 根据请求路径返回不同的模拟数据
          if (path === '/api/status') {
            // 模拟设备状态数据
            resolve({
              freezer_temp: -18.5,      // 模拟当前温度
              freezer_setpoint: -18,    // 模拟设定温度
              mode: '制冷',             // 模拟当前模式
              compressor: false,        // 模拟压缩机状态（关闭）
              fan: 0,                   // 模拟风扇状态（关闭）
              defrost: 'OFF'            // 模拟除霜状态（关闭）
            });
          } else {
            // 其他接口统一返回成功
            resolve({ success: true });
          }
        }, 300);
        return;  // 终止函数，不再执行真实请求
      }
      
      // ==================== 真实设备请求逻辑 ====================
      // 获取设备IP地址，如果全局没有则使用默认值（本地仿真地址）
      const app = getApp();
      const ip = app.globalData.deviceIP || '127.0.0.1:8180';
      // 构建完整的基地址：如果IP已经包含协议头（如http://）则直接使用，否则自动添加
      const baseUrl = ip.includes('://') ? ip : `http://${ip}`;
      
      // 使用微信小程序的网络请求API
      wx.request({
        url: `${baseUrl}${path}`,  // 完整的请求URL（如：http://192.168.1.100:8180/api/status）
        method: method,            // 请求方法（GET/POST/PUT/DELETE）
        data: data,                // 请求数据（POST方法时使用）
        timeout: 5000,             // 请求超时时间（5秒），防止无限等待
        success: (res) => {
          // 请求成功回调：HTTP状态码为200表示请求成功
          if (res.statusCode === 200) {
            // 将返回的数据传递给Promise的resolve函数
            resolve(res.data);
          } else {
            // HTTP状态码非200，表示请求失败（如：404、500等）
            reject(`HTTP ${res.statusCode}`);
          }
        },
        fail: (err) => {
          // 请求失败回调：网络错误、超时、设备离线等
          reject(err.errMsg || '网络错误');
        }
      });
    });
  }
});
