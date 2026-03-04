using System;
using System.Diagnostics;
using System.Drawing;
using System.IO.Ports;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;
using Timer = System.Windows.Forms.Timer;


namespace US100_RP2040_App
{
    public partial class Form1 : Form
    {
        private SerialPort serialPort;
        private bool isTesting = false;
        private int sentCount = 0;
        private int receivedCount = 0;

        private string lastPortName = null;
        private Timer portMonitorTimer;
        private string[] previousPorts = Array.Empty<string>();

        // Các control được tạo bằng code
        private ComboBox cboPorts;
        private Button btnConnect;
        private bool hasEverConnected = false;


        public Form1()
        {
            InitializeComponent(); // Bắt buộc: Gọi giao diện từ file Designer
            InitializeCustomComponents(); // Thêm các control tự tạo (ComboBox, nút Connect)
            LoadSerialPorts();

            serialPort = new SerialPort();
            serialPort.DataReceived += SerialPort_DataReceived;

            // --- Khởi tạo Timer để theo dõi cổng COM ---
            portMonitorTimer = new Timer();
            portMonitorTimer.Interval = 1000; // kiểm tra mỗi 1 giây
            portMonitorTimer.Tick += PortMonitorTimer_Tick;
            portMonitorTimer.Start();
        }

        /// <summary>
        /// Hàm này khởi tạo các control không có trong file Designer gốc.
        /// </summary>
        private void InitializeCustomComponents()
        {
            // --- ComboBox để chọn cổng COM ---
            this.cboPorts = new ComboBox();
            this.cboPorts.Location = new Point(12, 25); // Điều chỉnh vị trí cho đẹp
            this.cboPorts.Size = new Size(121, 21);
            this.cboPorts.DropDownStyle = ComboBoxStyle.DropDownList;
            this.Controls.Add(this.cboPorts);

            // --- Nút Kết nối / Ngắt kết nối ---
            this.btnConnect = new Button();
            this.btnConnect.Location = new Point(139, 23); // Điều chỉnh vị trí cho đẹp
            this.btnConnect.Size = new Size(90, 25);
            this.btnConnect.Text = "Kết nối";
            this.btnConnect.Font = new Font("Segoe UI", 9F, FontStyle.Bold);
            this.btnConnect.BackColor = Color.DodgerBlue;
            this.btnConnect.ForeColor = Color.White;
            this.btnConnect.Click += new EventHandler(this.btnConnect_Click);
            this.Controls.Add(this.btnConnect);
        }

        private void LoadSerialPorts()
        {
            cboPorts.Items.Clear();
            string[] ports = SerialPort.GetPortNames();
            if (ports.Length > 0)
            {
                cboPorts.Items.AddRange(ports);
                cboPorts.SelectedIndex = 0;
            }
            else
            {
                lblStatus.Text = "Không tìm thấy cổng COM nào.";
                lblStatus.ForeColor = Color.Red;
            }
        }

        // ===============================================================
        // 🔧 HÀM MỚI: Theo dõi thay đổi cổng COM và tự động kết nối lại
        // ===============================================================
        private void PortMonitorTimer_Tick(object sender, EventArgs e)
        {
            string[] currentPorts = SerialPort.GetPortNames();

            // 1️⃣ Cập nhật danh sách COM nếu có thay đổi
            if (!currentPorts.SequenceEqual(previousPorts))
            {
                cboPorts.Items.Clear();
                cboPorts.Items.AddRange(currentPorts);

                if (currentPorts.Length > 0)
                {
                    if (cboPorts.SelectedItem == null)
                        cboPorts.SelectedIndex = 0;

                    lblStatus.Text = $"🔌 Phát hiện {currentPorts.Length} cổng COM: {string.Join(", ", currentPorts)}";
                    lblStatus.ForeColor = Color.DarkBlue;
                }
                else
                {
                    lblStatus.Text = "⚠️ Không phát hiện thiết bị nào.";
                    lblStatus.ForeColor = Color.Red;
                }

                previousPorts = currentPorts;
            }

            // 2️⃣ Nếu đang kết nối mà cổng bị mất → đóng lại
            if (serialPort.IsOpen && !currentPorts.Contains(serialPort.PortName))
            {
                serialPort.Close();
                lblStatus.Text = "⚠️ Mất kết nối - đang chờ thiết bị...";
                lblStatus.ForeColor = Color.OrangeRed;
                btnConnect.Text = "Kết nối";
                btnConnect.BackColor = Color.DodgerBlue;
                return;
            }

            // 3️⃣ Nếu chưa kết nối mà có COM mới → tự động kết nối
            // 3️⃣ Nếu chưa kết nối mà có COM mới → tự động kết nối (kể cả lần đầu)
            // 3️⃣ Tự động kết nối lại chỉ khi trước đó đã kết nối ít nhất 1 lần
            if (!serialPort.IsOpen && hasEverConnected && currentPorts.Length > 0)
            {
                string targetPort = lastPortName;

                // Nếu cổng trước đó bị mất, tìm lại trong danh sách mới
                if (string.IsNullOrEmpty(targetPort) || !currentPorts.Contains(targetPort))
                    targetPort = currentPorts[0];

                try
                {
                    serialPort.PortName = targetPort;
                    serialPort.BaudRate = 9600;
                    serialPort.Open();

                    lastPortName = serialPort.PortName;
                    lblStatus.Text = $"🔄 Đã tự động kết nối lại: {serialPort.PortName}";
                    lblStatus.ForeColor = Color.DarkGreen;
                    btnConnect.Text = "Ngắt kết nối";
                    btnConnect.BackColor = Color.OrangeRed;

                    cboPorts.SelectedItem = targetPort;
                }
                catch
                {
                    lblStatus.Text = $"⚠️ Không thể mở lại {targetPort}.";
                    lblStatus.ForeColor = Color.Red;
                }
            }


        }

        // ===============================================================
        // PHẦN GỐC GIỮ NGUYÊN
        // ===============================================================

        private void btnConnect_Click(object sender, EventArgs e)
        {
            if (serialPort.IsOpen)
            {
                serialPort.Close();
                btnConnect.Text = "Kết nối";
                btnConnect.BackColor = Color.DodgerBlue;
                lblStatus.Text = "Đã ngắt kết nối.";
                lblStatus.ForeColor = Color.Red;
            }
            else
            {
                if (cboPorts.SelectedItem == null) { MessageBox.Show("Vui lòng chọn một cổng COM.", "Lỗi", MessageBoxButtons.OK, MessageBoxIcon.Error); return; }
                try
                {
                    serialPort.PortName = cboPorts.SelectedItem.ToString();
                    serialPort.BaudRate = 9600;
                    serialPort.Open();
                    btnConnect.Text = "Ngắt kết nối";
                    btnConnect.BackColor = Color.OrangeRed;
                    lblStatus.Text = $"Đã kết nối tới {serialPort.PortName}. Chờ lệnh...";
                    lblStatus.ForeColor = Color.DarkGreen;
                }
                catch (Exception ex) { MessageBox.Show($"Không thể kết nối: {ex.Message}", "Lỗi kết nối", MessageBoxButtons.OK, MessageBoxIcon.Error); }
            }
        }

        private void btnDo_Click(object sender, EventArgs e)
        {
            if (!serialPort.IsOpen) { MessageBox.Show("Vui lòng kết nối tới cổng COM trước.", "Thông báo", MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            isTesting = false;
            string command = "READ";
            serialPort.WriteLine(command);
            txtResponse.AppendText($"[GỬI] -> {command}\r\n");
            lblStatus.Text = "Đã gửi lệnh READ. Đang chờ dữ liệu khoảng cách...";
        }

        private async void btnTest_Click(object sender, EventArgs e)
        {
            if (!serialPort.IsOpen) { MessageBox.Show("Vui lòng kết nối tới cổng COM trước.", "Thông báo", MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }

            SetButtonsEnabled(false);
            isTesting = true;
            sentCount = 0;
            receivedCount = 0;
            txtFirmware.Clear();
            txtResponse.Clear();

            serialPort.WriteLine("TEST");
            txtResponse.AppendText("[GỬI] -> TEST\r\n");
            lblStatus.Text = "Bắt đầu bài test... Đang gửi 1000 gói tin có CRC.";

            await Task.Delay(200);

            for (int i = 0; i < 1000; i++)
            {
                string payload = $"hello_{i + 1}";
                int crcValue = Crc(payload);
                string packet = $"{payload}#{crcValue}";
                serialPort.WriteLine(packet);
                sentCount++;
                lblSendRate.Text = $"Đã gửi: {sentCount}";
                txtResponse.AppendText($"[GỬI] -> {packet}\r\n");
                await Task.Delay(35);
            }

            txtResponse.AppendText($"--- Đã gửi xong {sentCount} gói tin ---\r\n");
            lblStatus.Text = "Test hoàn tất. Đang chờ các gói tin phản hồi cuối cùng...";
            await Task.Delay(2000);
            isTesting = false;
            SetButtonsEnabled(true);
            lblStatus.Text = "Bài test đã kết thúc.";
            UpdateSuccessRate();
        }

        private void btnRestart_Click(object sender, EventArgs e)
        {
            if (serialPort.IsOpen)
            {
                isTesting = false;
                serialPort.WriteLine("STOP");
                txtResponse.AppendText("[GỬI] -> STOP\r\n");
            }
            txtFirmware.Clear();
            txtResponse.Clear();
            lblDistance.Text = "Khoảng cách: -- cm";
            lblReceived.Text = "Đã nhận: 0";
            lblSendRate.Text = "Đã gửi: 0";
            lblSuccessRate.Text = "Tỉ lệ gửi đúng: --";
            sentCount = 0;
            receivedCount = 0;
            lblStatus.Text = "Đã reset. Chờ lệnh...";
        }

        private void btnExit_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        private void SetButtonsEnabled(bool enabled)
        {
            btnDo.Enabled = enabled;
            btnTest.Enabled = enabled;
            btnRestart.Enabled = enabled;
            btnConnect.Enabled = enabled;
        }

        #region Serial Port and Data Handling
        private int Crc(string s)
        {
            int sum = 0;
            foreach (char c in s) { sum += (byte)c; }
            return sum % 256;
        }

        private void SerialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                while (serialPort.BytesToRead > 0)
                {
                    string data = serialPort.ReadLine().Trim();
                    this.Invoke(new Action(() => ProcessReceivedData(data)));
                }
            }
            catch (Exception ex) { Debug.WriteLine($"Lỗi khi nhận dữ liệu: {ex.Message}"); }
        }

        private void ProcessReceivedData(string data)
        {
            if (string.IsNullOrEmpty(data)) return;
            txtFirmware.AppendText($"[NHẬN] <- {data}\r\n");

            if (isTesting)
            {
                if (data.StartsWith("hello_"))
                {
                    receivedCount++;
                    lblReceived.Text = $"Đã nhận: {receivedCount}";
                    UpdateSuccessRate();
                }
            }
            else
            {
                if (data.StartsWith("@DIST_") && data.Contains("_#"))
                {
                    try
                    {
                        int crcSeparatorIndex = data.LastIndexOf("_#");
                        string core = data.Substring(0, crcSeparatorIndex + 2);
                        int receivedCrc = int.Parse(data.Substring(crcSeparatorIndex + 2));
                        if (receivedCrc == Crc(core))
                        {
                            string distanceStr = core.Replace("@DIST_", "").Replace("_#", "");
                            float distance = float.Parse(distanceStr, System.Globalization.CultureInfo.InvariantCulture);
                            lblDistance.Text = $"Khoảng cách: {distance:F1} cm";
                            lblStatus.Text = $"Nhận thành công frame lúc {DateTime.Now:HH:mm:ss}";
                            lblStatus.ForeColor = Color.DarkGreen;
                        }
                        else
                        {
                            lblStatus.Text = $"LỖI CRC!";
                            lblStatus.ForeColor = Color.Red;
                        }
                    }
                    catch (Exception ex) { lblStatus.Text = $"Lỗi khi xử lý frame: {ex.Message}"; }
                }
            }
        }

        private void UpdateSuccessRate()
        {
            if (sentCount > 0)
            {
                double rate = ((double)receivedCount / sentCount) * 100.0;
                lblSuccessRate.Text = $"Tỉ lệ gửi đúng: {rate:F1}%";
            }
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (serialPort.IsOpen) { serialPort.Close(); }
        }
        #endregion
    }
}