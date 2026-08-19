const API_URL =
  "https://script.google.com/macros/s/AKfycbz8OejMxvwfoRDuUM_P3r3jjU2FR_6hI6HCoiUzfBcPUW8HNhZoIpbuxaIX7s7p3BOo-Q/exec";

let xChart;
let yChart;
let isFirstLoad = true;
let currentStation = localStorage.getItem("currentStation") || 1;
let cntAcknowledged = 0;
let isTiltAlertShowing = false;
let isBatteryAlertShowing = false;
const TILT_LAST_SENT_KEY = "tilt_last_sent";
const TILT_ACK_KEY = "tilt_ack";
const TILT_ALERT_KEY = "tilt_alert";
const BATTERY_LAST_SENT_KEY = "battery_last_sent";
const BATTERY_ACK_KEY = "battery_ack";
const BATTERY_ALERT_KEY = "battery_alert";
const RESEND_INTERVAL_MS = 24 * 60 * 60 * 1000; // 24 hours
const NotificationElement = document.querySelector(".notification-empty");
let listwarning = [[], [], []];
let warningsIntervalId = null;
const AngleThreshold = 0.5;
// Global tilt thresholds (computed from origin inputs + 0.5)
let TILT_THRESHOLD_X =
  parseFloat(localStorage.getItem("TILT_THRESHOLD_X")) || 0;
let TILT_THRESHOLD_Y =
  parseFloat(localStorage.getItem("TILT_THRESHOLD_Y")) || 0;

// const ServiceID = "service_ssvavsi";
// const Battery_Alert_ID = "template_2l7t3yj";
// const Tilt_Alert_ID = "template_y9hpo5h";

const ServiceID = "service_nm5nxbh";
const Battery_Alert_ID = "template_iyqogfr";
const Tilt_Alert_ID = "template_ca6jr1e";

// ===== PARSE DATE =====
function parseDate(dateStr, timeStr) {
  if (!dateStr || !timeStr) return new Date();

  try {
    const d = dateStr.includes("T") ? dateStr.split("T")[0] : dateStr;
    const t = timeStr.includes("T")
      ? timeStr.split("T")[1].split(".")[0]
      : timeStr;

    // thêm timezone Việt Nam
    return new Date(`${d}T${t}+07:00`);
  } catch (e) {
    return new Date();
  }
}

function formatFull(date) {
  return date.toLocaleString("vi-VN");
}

function formatDateOnly(date) {
  return date.toLocaleDateString("vi-VN");
}

function getMinMax(arr, paddingPercent = 0.5) {
  if (!arr.length) return { min: 0, max: 10 };

  const minVal = Math.min(...arr);
  const maxVal = Math.max(...arr);

  const range = maxVal - minVal;

  // tránh trường hợp tất cả giá trị bằng nhau
  const padding =
    range === 0 ? Math.abs(maxVal) * 0.1 || 1 : range * paddingPercent;

  return {
    min: minVal - padding,
    max: maxVal + padding,
  };
}

// ===== LOAD DATA =====
async function loadSheetData(station = 1) {
  try {
    const res = await fetch(`${API_URL}?mode=read&station=${station}`);
    const data = await res.json();
    console.log("đang đọc station ", station);
    // console.log(data);

    if (!Array.isArray(data) || !data.length) return;

    data.sort((a, b) => parseDate(a.date, a.time) - parseDate(b.date, b.time));

    const labels = [];
    const xValues = [];
    const yValues = [];
    let latestBattery = "N/A";

    // use global tilt thresholds (ensure numeric)
    const thresholdX = parseFloat(TILT_THRESHOLD_X) || 0;
    const thresholdY = parseFloat(TILT_THRESHOLD_Y) || 0;

    data.forEach((row) => {
      if (row.x === "" || row.y === "") return;

      const xValue = Number(row.x);
      const yValue = Number(row.y);

      labels.push(new Date(row.datetime));
      xValues.push(Math.abs(xValue));
      yValues.push(Math.abs(yValue));

      if (row.vbat !== "") {
        const val = row.vbat;
        if (!isNaN(val)) {
          latestBattery = val;
        }
      }
    });

    // Compute tilt thresholds from first recorded values (first element) + 0.5
    if (xValues.length > 0 && yValues.length > 0) {
      const firstX = Math.abs(xValues[0]);
      const firstY = Math.abs(yValues[0]);

      const tiltX =
        typeof firstX === "number" && !isNaN(firstX)
          ? firstX
          : TILT_THRESHOLD_X;
      const tiltY =
        typeof firstY === "number" && !isNaN(firstY)
          ? firstY
          : TILT_THRESHOLD_Y;

      TILT_THRESHOLD_X = tiltX.toFixed(2);
      TILT_THRESHOLD_Y = tiltY.toFixed(2);

      localStorage.setItem("TILT_THRESHOLD_X", TILT_THRESHOLD_X);
      localStorage.setItem("TILT_THRESHOLD_Y", TILT_THRESHOLD_Y);
    }
    const batteryElement = document.querySelector(".battery-voltage");
    const percentBatteryElement = document.querySelector(".battery-percent");

    if (batteryElement && percentBatteryElement) {
      batteryElement.textContent = `${latestBattery}v`;
      const v = parseFloat(latestBattery);

      let percent = ((v - 3.3) / (4.2 - 3.3)) * 100;

      // giới hạn 0 → 100
      percent = Math.max(0, Math.min(100, percent));

      // hiển thị
      document.querySelector(".battery-percent").textContent =
        percent.toFixed(0) + "%";

      // đổi màu
      if (percent > 50) {
        percentBatteryElement.style.color = "#10b981";
        batteryElement.style.color = "#10b981";
        listwarning[station - 1][1] = "";
      } else if (percent > 20) {
        percentBatteryElement.style.color = "#f59e0b";
        batteryElement.style.color = "#f59e0b";
        listwarning[station - 1][1] = "";
      } else {
        percentBatteryElement.style.color = "#ff4d4d";
        batteryElement.style.color = "#ff4d4d";
        listwarning[station - 1][1] =
          "Please replace the battery of station " + currentStation + " ";
        // handle battery alert (anti-spam + 24h resend)
        const batteryPercent = percent;
        handleBatteryAlert(batteryPercent <= 20, batteryPercent, station);
      }
    }

    updateCharts(labels, xValues, yValues);

    // ===== CHECK LAST VALUE ONLY (use moving average of up to 10 points) =====
    const lastIndex = xValues.length - 1;

    if (lastIndex >= 0) {
      const lastX = xValues[lastIndex];
      const lastY = yValues[lastIndex];

      function avgLastN(arr, idx, n = 10) {
        let sum = 0;
        let cnt = 0;
        const start = Math.max(0, idx - n + 1);
        for (let i = start; i <= idx; i++) {
          const v = arr[i];
          if (v === null || typeof v === "undefined" || isNaN(v)) continue;
          sum += v;
          cnt++;
        }
        return cnt ? sum / cnt : arr[idx] || 0;
      }

      const avgX = avgLastN(xValues, lastIndex, 10);
      const avgY = avgLastN(yValues, lastIndex, 10);

      const isOver =
        Math.abs(avgX - thresholdX) > AngleThreshold ||
        Math.abs(avgY - thresholdY) > AngleThreshold;

      const alertData = {
        x: lastX,
        y: lastY,
        timestamp: labels[lastIndex].toLocaleString("vi-VN"),
      };

      // handle tilt alert with anti-spam logic
      handleTiltAlert(isOver, alertData, station);

      // Nếu KHÔNG vượt ngưỡng → reset để lần sau có thể alert lại
    }
  } catch (error) {
    console.error("Lỗi khi tải dữ liệu:", error);
  }
}

// ===== SHOW ALERT FROM STORAGE =====
function showAlertFromStorage(type = "tilt") {
  const key = type === "battery" ? BATTERY_ALERT_KEY : TILT_ALERT_KEY;
  const alertData = JSON.parse(localStorage.getItem(key));
  if (!alertData) return;

  let message = "";
  if (type === "battery") {
    message = `Battery warning for station ${currentStation}: ${alertData.percent || "N/A"}% (${alertData.timestamp || ""})`;
  } else {
    message = `Warning: Trạm ${currentStation} đang vượt quá ngưỡng cho phép. ${alertData.timestamp}`;
  }

  showAlert(message, type);
}

// Handle tilt alert with anti-spam (24h) and acknowledgement logic
function handleTiltAlert(isOver, alertData, station) {
  if (isOver) {
    localStorage.setItem(TILT_ALERT_KEY, JSON.stringify(alertData));

    const lastSent = localStorage.getItem(TILT_LAST_SENT_KEY);
    const ack = localStorage.getItem(TILT_ACK_KEY) === "1";

    // show the alert only once (unless user closed/acknowledged)
    if (!ack && !isTiltAlertShowing) {
      showAlertFromStorage("tilt");
      isTiltAlertShowing = true;
    }

    if (ack) {
      return; // user acknowledged/closed, do not send until condition clears
    }

    const now = Date.now();
    if (!lastSent) {
      sendTiltAlertEmail(station);
      localStorage.setItem(TILT_LAST_SENT_KEY, new Date(now).toISOString());
      listwarning[station - 1][0] =
        "Please check the station " + currentStation + " !";
      return;
    }

    const last = new Date(lastSent).getTime();
    if (now - last >= RESEND_INTERVAL_MS) {
      sendTiltAlertEmail(station);
      localStorage.setItem(TILT_LAST_SENT_KEY, new Date(now).toISOString());
      listwarning[station - 1][0] =
        "Please check the station " + currentStation + " !";
    } else {
      // within 24h, do not resend email
    }
  } else {
    // back to normal: clear stored state so next crossing behaves as first-time
    localStorage.removeItem(TILT_LAST_SENT_KEY);
    localStorage.removeItem(TILT_ACK_KEY);
    localStorage.removeItem(TILT_ALERT_KEY);
    isTiltAlertShowing = false;
    listwarning[station - 1][0] = "";
  }
}

function handleBatteryAlert(isLow, batteryPercent, station) {
  if (isLow) {
    const data = {
      percent: batteryPercent,
      timestamp: new Date().toLocaleString("vi-VN"),
    };
    localStorage.setItem(BATTERY_ALERT_KEY, JSON.stringify(data));

    const lastSent = localStorage.getItem(BATTERY_LAST_SENT_KEY);
    const ack = localStorage.getItem(BATTERY_ACK_KEY) === "1";

    // show only once until user closes
    if (!ack && !isBatteryAlertShowing) {
      showAlertFromStorage("battery");
      isBatteryAlertShowing = true;
    }

    if (ack) return; // user closed previously

    const now = Date.now();
    if (!lastSent) {
      sendBatteryAlertEmail(station);
      localStorage.setItem(BATTERY_LAST_SENT_KEY, new Date(now).toISOString());
      return;
    }

    const last = new Date(lastSent).getTime();
    if (now - last >= RESEND_INTERVAL_MS) {
      sendBatteryAlertEmail(station);
      localStorage.setItem(BATTERY_LAST_SENT_KEY, new Date(now).toISOString());
    }
  } else {
    localStorage.removeItem(BATTERY_LAST_SENT_KEY);
    localStorage.removeItem(BATTERY_ACK_KEY);
    localStorage.removeItem(BATTERY_ALERT_KEY);
    isBatteryAlertShowing = false;
    listwarning[station - 1][1] = "";
  }
}

// // ===== SEND ALERT EMAIL =====
function sendBatteryAlertEmail(station) {
  emailjs
    .send(
      ServiceID,

      Battery_Alert_ID,

      {
        station: station,
      },
    )

    .then(function (response) {
      console.log("EMAIL SUCCESS");

      console.log(response);
    })

    .catch(function (error) {
      console.log("EMAIL ERROR");

      console.log(error);
    });
}

function sendTiltAlertEmail(station) {
  emailjs
    .send(
      ServiceID,

      Tilt_Alert_ID,

      {
        station: station,
      },
    )

    .then(function (response) {
      console.log("EMAIL SUCCESS");

      console.log(response);
    })

    .catch(function (error) {
      console.log("EMAIL ERROR");

      console.log(error);
    });
}

// ===== UPDATE =====
function updateCharts(labels, xValues, yValues) {
  const xMM = getMinMax(xValues, 0.5) + 5;
  const yMM = getMinMax(yValues, 0.5) + 5;

  const totalPoints = labels.length;

  // ===== THÊM ĐIỂM GIẢ =====
  const paddingPoints = 5;

  const fakeLabels = [];
  const fakeX = [];
  const fakeY = [];

  if (labels.length > 0) {
    const lastTime = labels[labels.length - 1];

    for (let i = 1; i <= paddingPoints; i++) {
      const newTime = new Date(lastTime.getTime() + i * 60000); // +1 phút
      fakeLabels.push(newTime);
      fakeX.push(null);
      fakeY.push(null);
    }
  }

  const fullLabels = labels.concat(fakeLabels);
  const fullX = xValues.concat(fakeX);
  const fullY = yValues.concat(fakeY);

  // ===== RESET DATA =====
  xChart.data.labels = [];
  xChart.data.datasets[0].data = [];
  yChart.data.labels = [];
  yChart.data.datasets[0].data = [];

  // ===== GÁN DATA =====
  xChart.data.labels = fullLabels;
  xChart.data.datasets[0].data = fullX;
  xChart.options.scales.y.min = xMM.min;
  xChart.options.scales.y.max = xMM.max;

  yChart.data.labels = fullLabels;
  yChart.data.datasets[0].data = fullY;
  yChart.options.scales.y.min = yMM.min;
  yChart.options.scales.y.max = yMM.max;

  // zoom vẫn giữ nguyên
  if (xChart.options.plugins.zoom) {
    xChart.options.plugins.zoom.limits = {
      x: { min: 0, max: fullLabels.length - 1 },
    };
    yChart.options.plugins.zoom.limits = {
      x: { min: 0, max: fullLabels.length - 1 },
    };
  }

  if (isFirstLoad && totalPoints > 0) {
    const displayRange = 50;
    const startIdx = Math.max(0, totalPoints - displayRange);
    const endIdx = fullLabels.length - 1;

    xChart.options.scales.x.min = startIdx;
    xChart.options.scales.x.max = endIdx;

    yChart.options.scales.x.min = startIdx;
    yChart.options.scales.x.max = endIdx;

    isFirstLoad = false;
  }

  xChart.update();
  yChart.update();
}

// ===== INIT =====
function initCharts() {
  // refresh thresholds from storage
  TILT_THRESHOLD_X =
    parseFloat(localStorage.getItem("TILT_THRESHOLD_X")) ||
    TILT_THRESHOLD_X ||
    0;
  TILT_THRESHOLD_Y =
    parseFloat(localStorage.getItem("TILT_THRESHOLD_Y")) ||
    TILT_THRESHOLD_Y ||
    0;
  console.log("Ngưỡng X:", TILT_THRESHOLD_X, "Ngưỡng Y:", TILT_THRESHOLD_Y);

  if (!TILT_THRESHOLD_X) {
    console.warn("Chưa có TILT_THRESHOLD_X được lưu trữ trong localStorage.");
  }

  if (!TILT_THRESHOLD_Y) {
    console.warn("Chưa có TILT_THRESHOLD_Y được lưu trữ trong localStorage.");
  }

  const baseOptionsX = {
    responsive: true,
    maintainAspectRatio: false,
    layout: { padding: { right: 20 } },
    plugins: {
      tooltip: {
        callbacks: {
          title: function (ctx) {
            const date = ctx[0].chart.data.labels[ctx[0].dataIndex];
            return formatFull(date);
          },
          label: function (ctx) {
            return "Giá trị: " + ctx.raw;
          },
        },
      },
      zoom: {
        pan: {
          enabled: true,
          mode: "x",
        },
        zoom: {
          wheel: { enabled: true },
          pinch: { enabled: true },
          mode: "x",
          drag: { enabled: false },
        },
      },
      annotation: {
        annotations: {
          xThreshold: {
            type: "line",
            yMin: TILT_THRESHOLD_X,
            yMax: TILT_THRESHOLD_X,
            borderColor: "rgb(255, 99, 132)",
            borderWidth: 2,
            borderDash: [6, 6],
            label: {
              display: true,
              content: "Original value X: " + TILT_THRESHOLD_X,
              position: "start",
              backgroundColor: "rgba(255, 99, 133, 0.62)",
              color: "white",
              font: { size: 12 },
            },
          },
        },
      },
    },
  };

  const baseOptionsY = {
    responsive: true,
    maintainAspectRatio: false,
    layout: { padding: { right: 20 } },
    plugins: {
      tooltip: {
        callbacks: {
          title: function (ctx) {
            const date = ctx[0].chart.data.labels[ctx[0].dataIndex];
            return formatFull(date);
          },
          label: function (ctx) {
            return "Giá trị: " + ctx.raw;
          },
        },
      },
      zoom: {
        pan: {
          enabled: true,
          mode: "x",
        },
        zoom: {
          wheel: { enabled: true },
          pinch: { enabled: true },
          mode: "x",
          drag: { enabled: false },
        },
      },
      annotation: {
        annotations: {
          yThreshold: {
            type: "line",
            yMin: TILT_THRESHOLD_Y,
            yMax: TILT_THRESHOLD_Y,
            borderColor: "rgb(255, 99, 132)",
            borderWidth: 2,
            borderDash: [6, 6],
            label: {
              display: true,
              content: "Original value Y: " + TILT_THRESHOLD_Y,
              position: "start",
              backgroundColor: "rgba(255, 99, 133, 0.62)",
              color: "white",
              font: { size: 12 },
            },
          },
        },
      },
    },
  };

  const xScaleConfig = {
    ticks: {
      autoSkip: true,
      maxTicksLimit: 8,
      maxRotation: 0,
      callback: function (value) {
        const labels = this.chart.data.labels;
        if (!labels || labels.length === 0 || !labels[value]) return "";

        const cur = labels[value];

        const timeStr = cur.toLocaleTimeString("vi-VN", {
          hour: "2-digit",
          minute: "2-digit",
        });

        const dateStr = cur.toLocaleDateString("vi-VN");

        return [timeStr, dateStr];
      },
    },
  };

  xChart = new Chart(document.getElementById("xChart"), {
    type: "line",
    data: {
      labels: [],
      datasets: [
        {
          label: "Giá trị X",
          data: [],
          tension: 0.3,
          borderColor: "blue",
          pointBackgroundColor: function (context) {
            const value = context.dataset.data[context.dataIndex];

            if (value === null) return "gray"; // điểm giả
            // moving average of up to 10 points (current + 9 previous)
            const idx = context.dataIndex;
            const arr = context.dataset.data;
            let sum = 0;
            let cnt = 0;
            for (let i = Math.max(0, idx - 9); i <= idx; i++) {
              const v = arr[i];
              if (v === null || typeof v === "undefined" || isNaN(v)) continue;
              sum += v;
              cnt++;
            }
            const avg = cnt ? sum / cnt : value;
            return Math.abs(avg - TILT_THRESHOLD_X) > AngleThreshold
              ? "red"
              : "blue";
          },
          pointRadius: function (context) {
            const value = context.dataset.data[context.dataIndex];

            if (value === null) return 0; // ẩn điểm giả
            // compute same moving average for radius
            const idx = context.dataIndex;
            const arr = context.dataset.data;
            let sum = 0;
            let cnt = 0;
            for (let i = Math.max(0, idx - 9); i <= idx; i++) {
              const v = arr[i];
              if (v === null || typeof v === "undefined" || isNaN(v)) continue;
              sum += v;
              cnt++;
            }
            const avg = cnt ? sum / cnt : value;
            return Math.abs(avg - TILT_THRESHOLD_X) > AngleThreshold ? 8 : 3;
          },
          segment: {
            borderColor: (ctx) => {
              const v = ctx.p1.parsed.y;
              if (v === null) return "rgba(150,150,150,0.3)";
              return "blue";
            },
          },
        },
      ],
    },
    options: {
      ...baseOptionsX,
      scales: {
        x: xScaleConfig,
        y: { title: { display: true, text: "Trục X" } },
      },
    },
  });

  yChart = new Chart(document.getElementById("yChart"), {
    type: "line",
    data: {
      labels: [],
      datasets: [
        {
          label: "Giá trị Y",
          data: [],
          tension: 0.3,
          borderColor: "blue",
          pointBackgroundColor: function (context) {
            const value = context.dataset.data[context.dataIndex];

            if (value === null) return "gray"; // điểm giả
            // moving average of up to 10 points (current + 9 previous)
            const idx = context.dataIndex;
            const arr = context.dataset.data;
            let sum = 0;
            let cnt = 0;
            for (let i = Math.max(0, idx - 9); i <= idx; i++) {
              const v = arr[i];
              if (v === null || typeof v === "undefined" || isNaN(v)) continue;
              sum += v;
              cnt++;
            }
            const avg = cnt ? sum / cnt : value;
            return Math.abs(avg - TILT_THRESHOLD_Y) > AngleThreshold
              ? "red"
              : "blue";
          },
          pointRadius: function (context) {
            const value = context.dataset.data[context.dataIndex];

            if (value === null) return 0; // ẩn điểm giả
            // compute same moving average for radius
            const idx = context.dataIndex;
            const arr = context.dataset.data;
            let sum = 0;
            let cnt = 0;
            for (let i = Math.max(0, idx - 9); i <= idx; i++) {
              const v = arr[i];
              if (v === null || typeof v === "undefined" || isNaN(v)) continue;
              sum += v;
              cnt++;
            }
            const avg = cnt ? sum / cnt : value;
            return Math.abs(avg - TILT_THRESHOLD_Y) > AngleThreshold ? 8 : 3;
          },
          segment: {
            borderColor: (ctx) => {
              const v = ctx.p1.parsed.y;
              if (v === null) return "rgba(150,150,150,0.3)";
              return "blue";
            },
          },
        },
      ],
    },
    options: {
      ...baseOptionsY,
      scales: {
        x: xScaleConfig,
        y: { title: { display: true, text: "Trục Y" } },
      },
    },
  });
}

// ===== START =====
window.onload = () => {
  initCharts();
  loadSheetData(currentStation);
  startWarningsRotation();
  selectStation(currentStation);
};

/**
 * Start rotating messages from `listwarning` into the `.notification-empty` element every 10s.
 * If `listwarning` is empty, shows "Empty !".
 */
function startWarningsRotation(intervalMs = 10000) {
  if (warningsIntervalId) return; // already running

  function showNext() {
    const el = document.querySelector(".notification-empty");
    if (
      !Array.isArray(listwarning[currentStation - 1]) ||
      listwarning[currentStation - 1].length === 0
    ) {
      if (el) {
        el.textContent = "Empty !";
        el.style.color = "black";
        el.style.fontWeight = "normal";
      }
      return;
    }

    // keep an index on the function object
    if (typeof showNext._idx === "undefined") showNext._idx = 0;

    const msg = listwarning[currentStation - 1][showNext._idx] || "";
    if (el) {
      el.textContent = msg;
      el.style.color = "red";
      el.style.fontWeight = "bold";
    }

    showNext._idx =
      (showNext._idx + 1) % listwarning[currentStation - 1].length;
  }

  // show immediately once
  showNext();

  warningsIntervalId = setInterval(showNext, intervalMs);
}

function selectStation(n) {
  isFirstLoad = true;
  currentStation = n;
  localStorage.setItem("currentStation", n);

  // ===== cập nhật active sidebar =====
  const items = document.querySelectorAll(".sidebar li");
  items.forEach((item, index) => {
    if (index === n - 1) {
      item.classList.add("active");
    } else {
      item.classList.remove("active");
    }
  });

  // ===== load data =====
  loadSheetData(currentStation);
}

document.addEventListener("DOMContentLoaded", () => {
  const modal = document.getElementById("myModal");
  const openBtn = document.getElementById("openPopupBtn");
  const closeBtn = document.querySelector(".close-btn");
  const saveBtn = document.getElementById("saveBtn");

  openBtn.onclick = () => {
    modal.style.display = "block";
  };

  closeBtn.onclick = () => {
    modal.style.display = "none";
  };

  window.onclick = (event) => {
    if (event.target == modal) {
      modal.style.display = "none";
    }
  };

  saveBtn.onclick = () => {
    const origin_x = document.getElementById("input_origin_x").value;
    const origin_y = document.getElementById("input_origin_y").value;

    const tiltX = origin_x !== "" ? parseFloat(origin_x) + 0.3 : 0;
    const tiltY = origin_y !== "" ? parseFloat(origin_y) + 0.3 : 0;

    localStorage.setItem("origin_x", origin_x);
    localStorage.setItem("origin_y", origin_y);
    localStorage.setItem("TILT_THRESHOLD_X", tiltX);
    localStorage.setItem("TILT_THRESHOLD_Y", tiltY);

    TILT_THRESHOLD_X = tiltX;
    TILT_THRESHOLD_Y = tiltY;

    alert(
      "Đã lưu: " +
        origin_x +
        " (TILT_X:" +
        tiltX +
        ") - " +
        origin_y +
        " (TILT_Y:" +
        tiltY +
        ")",
    );

    modal.style.display = "none";
  };
  loadSheetData(currentStation);
});

function handleChangeProperty() {
  console.log("Đã bấm vô đây");
  const thresholdValue = document.getElementById("input1").value;
  if (thresholdValue === "") {
    alert("Vui lòng nhập giá trị ngưỡng!");
    return;
  }

  localStorage.setItem("threshold", thresholdValue);

  console.log("Đã lưu giá trị ngưỡng:", thresholdValue);
  alert("Lưu thành công!");
}

document.addEventListener("DOMContentLoaded", () => {
  const saveBtn = document.getElementById("saveBtn");
  if (saveBtn) {
    saveBtn.addEventListener("click", handleChangeProperty);
  }
});

/**
 * Hàm để hiển thị thông báo với một con số cụ thể
 * @param {number} value - Con số muốn hiển thị
 */
function showAlert(value, type = "tilt") {
  const container = document.getElementById("alertContainer");

  const alertItem = document.createElement("div");
  alertItem.className = "alert-item";

  alertItem.innerHTML = `
        <i class="fa-solid fa-triangle-exclamation"></i>
        <div class="alert-content">
            <span class="alert-number">${value}</span>
        </div>
        <button class="close-btn">&times;</button>
    `;

  // mark type for close handler
  alertItem.dataset.type = type;

  const closeBtn = alertItem.querySelector(".close-btn");
  closeBtn.onclick = () => {
    alertItem.classList.add("hide");
    setTimeout(() => alertItem.remove(), 300);
    NotificationElement.textContent = "Empty !";
    NotificationElement.style.color = "black";
    // acknowledge this alert type so we won't resend until condition clears
    try {
      if (type === "tilt") {
        localStorage.setItem(TILT_ACK_KEY, "1");
        isTiltAlertShowing = false;
      } else if (type === "battery") {
        localStorage.setItem(BATTERY_ACK_KEY, "1");
        isBatteryAlertShowing = false;
      }
    } catch (e) {
      console.warn("Could not set ack flag:", e);
    }
  };

  container.appendChild(alertItem);
}

/**
 * Hàm tắt thông báo (chỉ tắt khi người dùng bấm nút X)
 */
function closeAlert() {
  const alertBox = document.getElementById("customAlert");
  alertBox.style.display = "none";
}

const handleLogout = () => {
  localStorage.setItem("isAuthenticated", false);
  location.reload();
};

// Tự động load mỗi 5 giây
setInterval(() => {
  loadSheetData(currentStation);
}, 5000);

const isLogin = JSON.parse(localStorage.getItem("isAuthenticated"));
if (!isLogin) {
  window.location.href = "./Login.html";
}

// Save values to localStorage
document.getElementById("saveBtn").addEventListener("click", function () {
  const originX = document.getElementById("input_origin_x").value;
  const originY = document.getElementById("input_origin_y").value;

  const tiltX = originX !== "" ? parseFloat(originX) + 0.3 : 0;
  const tiltY = originY !== "" ? parseFloat(originY) + 0.3 : 0;

  localStorage.setItem("origin_x", originX);
  localStorage.setItem("origin_y", originY);
  localStorage.setItem("TILT_THRESHOLD_X", tiltX);
  localStorage.setItem("TILT_THRESHOLD_Y", tiltY);
  TILT_THRESHOLD_X = tiltX;
  TILT_THRESHOLD_Y = tiltY;
});

// Load values from localStorage when opening the modal
document.getElementById("openPopupBtn").addEventListener("click", function () {
  const originX = localStorage.getItem("origin_x") || "";
  const originY = localStorage.getItem("origin_y") || "";
  const tiltX = parseFloat(localStorage.getItem("TILT_THRESHOLD_X"));
  const tiltY = parseFloat(localStorage.getItem("TILT_THRESHOLD_Y"));

  document.getElementById("input_origin_x").value = originX;
  document.getElementById("input_origin_y").value = originY;

  // ensure globals are in sync
  TILT_THRESHOLD_X = !isNaN(tiltX)
    ? tiltX
    : originX
      ? parseFloat(originX) + AngleThreshold
      : 0;
  TILT_THRESHOLD_Y = !isNaN(tiltY)
    ? tiltY
    : originY
      ? parseFloat(originY) + AngleThreshold
      : 0;
});
