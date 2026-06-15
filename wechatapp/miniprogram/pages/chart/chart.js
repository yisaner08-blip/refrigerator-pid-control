// 对应的是数据可视化界面，负责以折线图形式展示冰箱温度的历史变化趋势，
// 支持选择不同时间范围（如最近 1 小时/24 小时）加载对应数据，
// 通过 Canvas 手动绘制带网格线和图例的温度曲线，同时计算并显示统计指标（最高温、最低温、平均值），
// 数据来源优先调用 /api/history 接口从设备获取真实历史记录，
// 设备未连接时自动降级为 Mock 模拟数据以保证界面可演示。
// pages/chart/chart.js
Page({
  // ==================== 页面数据初始化 ====================
  data: {
    // 时间范围选择：'1h'=1小时，'6h'=6小时，'24h'=24小时，'7d'=7天
    timeRange: '1h',
    // 冷冻室温度统计数据（初始值）
    freezerHigh: -15,    // 最高温度
    freezerLow: -22,     // 最低温度
    freezerAvg: -18,     // 平均温度
    // 图表数据对象
    chartData: {
      freezer: [],   // 冷冻室温度数组
      labels: []      // 时间标签数组
    }
  },

  // ==================== 生命周期函数 ====================
  // 页面加载时执行，只执行一次
  onLoad() {
    // 加载图表数据，页面打开时自动获取温度历史数据
    this.loadChartData();
  },

  // 页面显示时执行，每次页面显示都会执行
  onShow() {
    // 页面显示时刷新图表
    // 这里可以添加定时刷新逻辑，比如每30秒自动刷新图表
  },

  // ==================== 时间范围选择函数 ====================
  // 时间范围选择
  onTimeRange(e) {
    // 从点击事件中获取用户选择的时间范围
    // e.currentTarget.dataset.range 对应WXML中的 data-range="Xh"
    const range = e.currentTarget.dataset.range;
    // 更新页面数据中的时间范围
    this.setData({ timeRange: range });
    // 重新加载对应时间范围的图表数据
    this.loadChartData();
  },

  // ==================== 加载图表数据函数 ====================
  // 加载图表数据
  async loadChartData() {
    try {
      // 获取小程序全局实例，用于访问全局数据
      const app = getApp();
      // 从全局数据中获取设备IP地址
      const deviceIP = app.globalData.deviceIP;
      
      // 安全检查：如果设备IP不存在，说明用户还未连接设备
      if (!deviceIP) {
        // 使用模拟数据
        this.generateMockData();
        // 绘制图表（使用模拟数据）
        this.drawChart();
        return;  // 终止函数，不再执行真实请求
      }

      // 从设备获取历史数据
      // '/api/history' 是后端提供的历史数据查询接口
      // 该接口返回温度历史数据和对应的时间标签
      const res = await this.requestAPI(deviceIP, '/api/history');
      
      // 将获取到的历史数据更新到页面数据中
      this.setData({
        chartData: {
          // 冷冻室温度数组，如果接口返回为空则使用空数组
          freezer: res.freezer || [],
          // 时间标签数组，如果接口返回为空则使用空数组
          labels: res.labels || []
        }
      });
      
      // 计算统计数据（最高温、最低温、平均温）
      this.calculateStats();
      // 绘制图表
      this.drawChart();
    } catch (error) {
      // 如果请求失败（网络错误、设备离线等），捕获异常并记录
      console.error('加载数据失败:', error);
      // 使用模拟数据
      this.generateMockData();
      // 绘制图表（使用模拟数据）
      this.drawChart();
    }
  },

  // ==================== 生成模拟数据函数 ====================
  // 生成模拟数据
  generateMockData() {
    // 初始化温度数组和时间标签数组
    const freezer = [];
    const labels = [];

    // 根据选择的时间范围确定数据点数
    const dataCount = this.data.timeRange === '1h' ? 12 :
                      this.data.timeRange === '6h' ? 24 :
                      this.data.timeRange === '24h' ? 24 : 168;  // 7天=168小时

    // 生成模拟温度数据
    for (let i = 0; i < dataCount; i++) {
      // 生成-18°C左右的随机温度（±2°C波动）
      freezer.push(-18 + (Math.random() - 0.5) * 4);
      // 生成时间标签（简化版，实际应根据时间范围生成）
      labels.push(`${i}:00`);
    }

    // 将生成的模拟数据更新到页面数据中
    this.setData({
      chartData: { freezer, labels }
    });

    // 计算模拟数据的统计数据
    this.calculateStats();
  },

  // ==================== 计算统计数据函数 ====================
  // 计算统计数据
  calculateStats() {
    // 从页面数据中解构出冷冻室温度数组
    const { freezer } = this.data.chartData;

    // 只有当温度数组有数据时才计算统计数据
    if (freezer.length > 0) {
      // 更新页面数据中的统计数据
      this.setData({
        // 计算最高温度，保留1位小数
        freezerHigh: Math.max(...freezer).toFixed(1),
        // 计算最低温度，保留1位小数
        freezerLow: Math.min(...freezer).toFixed(1),
        // 计算平均温度：先求和再除以数据点数，保留1位小数
        freezerAvg: (freezer.reduce((a, b) => a + b, 0) / freezer.length).toFixed(1)
      });
    }
  },

  // ==================== 绘制图表函数 ====================
  // 绘制图表
  drawChart() {
    // 创建画布上下文，'tempChart'是WXML中canvas组件的id
    const ctx = wx.createCanvasContext('tempChart');
    // 从页面数据中解构出温度数组和时间标签数组
    const { freezer, labels } = this.data.chartData;

    // 安全检查：如果温度数组为空，不绘制图表
    if (freezer.length === 0) return;

    // 定义画布尺寸和边距
    const width = 300;    // 画布宽度
    const height = 200;   // 画布高度
    const padding = 40;   // 边距（用于显示坐标轴标签）

    // 清空画布，准备绘制新图表
    ctx.clearRect(0, 0, width, height);

    // ==================== 绘制网格线 ====================
    // 设置网格线样式：浅灰色、细线
    ctx.setStrokeStyle('#f0f0f0');
    ctx.setLineWidth(0.5);
    // 绘制10条水平网格线
    for (let i = 0; i <= 10; i++) {
      // 计算每条网格线的Y坐标
      const y = padding + (height - 2 * padding) * i / 10;
      ctx.beginPath();
      // 从左边距开始
      ctx.moveTo(padding, y);
      // 到右边距结束
      ctx.lineTo(width - padding, y);
      ctx.stroke();
    }

    // ==================== 绘制温度曲线 ====================
    // 设置温度曲线样式：蓝色、粗线
    ctx.setStrokeStyle('#2b5aed');
    ctx.setLineWidth(2);
    ctx.beginPath();
    // 遍历温度数组，绘制温度曲线
    freezer.forEach((temp, index) => {
      // 计算数据点的X坐标（等间距分布）
      const x = padding + (width - 2 * padding) * index / (freezer.length - 1);
      // 计算数据点的Y坐标（将温度映射到画布高度）
      // 温度范围假设为-25°C到25°C，这里需要根据实际温度范围调整
      const y = padding + (height - 2 * padding) * (1 - (temp + 25) / 50);
      // 如果是第一个点，移动到该位置；否则画线到该位置
      if (index === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.stroke();

    // ==================== 绘制图例 ====================
    // 设置图例文字样式
    ctx.setFillStyle('#333');
    ctx.setFontSize(10);
    // 绘制图例文字
    ctx.fillText('冷冻室', width - 60, padding + 15);
    // 绘制图例颜色块
    ctx.setFillStyle('#2b5aed');
    ctx.fillRect(width - 90, padding + 8, 15, 2);
    // 将绘制内容输出到画布
    ctx.draw();
  },

  // ==================== 刷新数据函数 ====================
  // 刷新数据
  onRefresh() {
    // 重新加载图表数据
    this.loadChartData();
    
    // 显示刷新成功提示
    wx.showToast({
      title: '数据已刷新',
      icon: 'success'
    });
  },

  // ==================== 请求API函数 ====================
  // 请求API
  requestAPI(ip, path) {
    // 返回Promise对象，支持async/await语法
    return new Promise((resolve, reject) => {
      // 使用微信小程序的网络请求API
      wx.request({
        // 构建完整的请求URL（如：http://192.168.1.100:8180/api/history）
        url: `http://${ip}${path}`,
        // 请求方法：GET（查询历史数据）
        method: 'GET',
        // 请求超时时间（5秒），防止无限等待
        timeout: 5000,
        // 请求成功回调
        success: (res) => {
          // HTTP状态码为200表示请求成功
          if (res.statusCode === 200) {
            // 将返回的数据传递给Promise的resolve函数
            resolve(res.data);
          } else {
            // HTTP状态码非200，表示请求失败
            reject(`HTTP ${res.statusCode}`);
          }
        },
        // 请求失败回调：网络错误、超时、设备离线等
        fail: (err) => {
          reject(err.errMsg || '网络错误');
        }
      });
    });
  }
});
