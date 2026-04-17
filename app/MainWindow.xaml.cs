using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace SidetoneApp
{
    public class DeviceItem
    {
        public string Name { get; set; }
        public string Id { get; set; }
    }

    public partial class MainWindow : Window
    {
        private bool _initialized = false;
        private bool _loadingValues = false;
        private bool _loadingDevices = false;

        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            MinimizeBtn.Content = "\uE949";
            MaximizeBtn.Content = "\uE739";
            CloseBtn.Content = "\uE8BB";

            foreach (var btn in new[] { MinimizeBtn, MaximizeBtn, CloseBtn })
            {
                btn.FontFamily = new System.Windows.Media.FontFamily("Segoe MDL2 Assets");
                btn.FontSize = 10;
            }

            Native.Sidetone_Init();

            LoadDevices();

            if (Native.Sidetone_IsActive() == 0)
            {
                ToggleButton.Content = "Start";
            }

            _loadingValues = true;

            int gainVal = Native.Sidetone_GetGainPercent();
            int suppressVal = Native.Sidetone_GetSuppressionPercent();
            int inputGainVal = Native.Sidetone_GetInputGainPercent();
            int gateVal = Native.Sidetone_GetNoiseGateThreshold();
            int bufferVal = Native.Sidetone_GetBufferSize();

            GainSlider.Value = gainVal;
            GainValue.Text = gainVal + "%";

            SuppressSlider.Value = suppressVal;
            SuppressValue.Text = suppressVal + "%";

            InputGainSlider.Value = inputGainVal;
            InputGainValue.Text = inputGainVal + "%";

            GateSlider.Value = gateVal;
            GateValue.Text = gateVal + "%";

            BufferSlider.Value = bufferVal;
            BufferValue.Text = bufferVal.ToString();
            double initMs = bufferVal * 1000.0 / 48000.0;
            BufferLatency.Text = $"{bufferVal} frames (~{initMs:F1}ms)";
            UpdateSampleRateDisplay();

            _loadingValues = false;
            _initialized = true;
        }

        private void LoadDevices()
        {
            _loadingDevices = true;

            int inputCount = Native.Sidetone_GetInputDeviceCount();
            for (int i = 0; i < inputCount; i++)
            {
                char[] id = new char[256];
                char[] name = new char[256];
                if (Native.Sidetone_GetInputDevice(i, id, name, 256) == 1)
                {
                    InputDeviceCombo.Items.Add(new DeviceItem
                    {
                        Name = new string(name).TrimEnd('\0'),
                        Id = new string(id).TrimEnd('\0')
                    });
                }
            }

            int outputCount = Native.Sidetone_GetOutputDeviceCount();
            for (int i = 0; i < outputCount; i++)
            {
                char[] id = new char[256];
                char[] name = new char[256];
                if (Native.Sidetone_GetOutputDevice(i, id, name, 256) == 1)
                {
                    OutputDeviceCombo.Items.Add(new DeviceItem
                    {
                        Name = new string(name).TrimEnd('\0'),
                        Id = new string(id).TrimEnd('\0')
                    });
                }
            }

            char[] currentInputId = new char[256];
            char[] currentOutputId = new char[256];
            Native.Sidetone_GetCurrentInputDevice(currentInputId, 256);
            Native.Sidetone_GetCurrentOutputDevice(currentOutputId, 256);

            string inputIdStr = new string(currentInputId).TrimEnd('\0');
            string outputIdStr = new string(currentOutputId).TrimEnd('\0');

            for (int i = 0; i < InputDeviceCombo.Items.Count; i++)
            {
                var item = (DeviceItem)InputDeviceCombo.Items[i];
                if (item.Id == inputIdStr || (string.IsNullOrEmpty(inputIdStr) && i == 0))
                {
                    InputDeviceCombo.SelectedIndex = i;
                    break;
                }
            }

            for (int i = 0; i < OutputDeviceCombo.Items.Count; i++)
            {
                var item = (DeviceItem)OutputDeviceCombo.Items[i];
                if (item.Id == outputIdStr || (string.IsNullOrEmpty(outputIdStr) && i == 0))
                {
                    OutputDeviceCombo.SelectedIndex = i;
                    break;
                }
            }

            _loadingDevices = false;
        }

        private void RestartIfRunning()
        {
            if (!_initialized) return;

            bool wasActive = Native.Sidetone_IsActive() != 0;
            if (wasActive)
            {
                Native.Sidetone_Stop();
                System.Threading.Thread.Sleep(100);
                Native.Sidetone_Start();
            }
        }

        private void UpdateSampleRateDisplay()
        {
            int inputRate = Native.Sidetone_GetInputSampleRate();
            int outputRate = Native.Sidetone_GetOutputSampleRate();
            SampleRateDisplay.Text = $"Input: {inputRate} Hz | Output: {outputRate} Hz";
            
            if (inputRate != outputRate) {
                SampleRateDisplay.Foreground = new System.Windows.Media.SolidColorBrush(
                    System.Windows.Media.Color.FromRgb(255, 100, 100));
            } else {
                SampleRateDisplay.Foreground = new System.Windows.Media.SolidColorBrush(
                    System.Windows.Media.Color.FromRgb(128, 128, 128));
            }
        }

        private void InputDeviceCombo_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (_loadingDevices || !_initialized) return;

            if (Native.Sidetone_IsActive() != 0) return;

            var selected = InputDeviceCombo.SelectedItem as DeviceItem;
            if (selected != null)
            {
                Native.Sidetone_SetInputDevice(selected.Id);
            }
        }

        private void OutputDeviceCombo_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (_loadingDevices || !_initialized) return;

            if (Native.Sidetone_IsActive() != 0) return;

            var selected = OutputDeviceCombo.SelectedItem as DeviceItem;
            if (selected != null)
            {
                Native.Sidetone_SetOutputDevice(selected.Id);
                UpdateSampleRateDisplay();
            }
        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            Native.Sidetone_Stop();
            Native.Sidetone_Shutdown();
        }

        private void ToggleButton_Click(object sender, RoutedEventArgs e)
        {
            if (Native.Sidetone_IsActive() != 0)
            {
                Native.Sidetone_SetActive(0);
                ToggleButton.Content = "Start";
                InputDeviceCombo.IsEnabled = true;
                OutputDeviceCombo.IsEnabled = true;
            }
            else
            {
                Native.Sidetone_Start();
                Native.Sidetone_SetActive(1);
                ToggleButton.Content = "Stop";
                InputDeviceCombo.IsEnabled = false;
                OutputDeviceCombo.IsEnabled = false;
            }
        }

        private void GainSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (!_initialized || _loadingValues) return;
            int val = (int)Math.Round(e.NewValue);
            GainValue.Text = val + "%";
            Native.Sidetone_SetGainPercent(val);
        }

        private void SuppressSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (!_initialized || _loadingValues) return;
            int val = (int)Math.Round(e.NewValue);
            SuppressValue.Text = val + "%";
            Native.Sidetone_SetSuppressionPercent(val);
        }

        private void InputGainSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (!_initialized || _loadingValues) return;
            int val = (int)Math.Round(e.NewValue);
            InputGainValue.Text = val + "%";
            Native.Sidetone_SetInputGainPercent(val);
        }

        private void GateSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (!_initialized || _loadingValues) return;
            int val = (int)Math.Round(e.NewValue);
            GateValue.Text = val + "%";
            Native.Sidetone_SetNoiseGateThreshold(val);
        }

        private void BufferSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (!_initialized || _loadingValues) return;
            int val = (int)Math.Round(e.NewValue);
            BufferValue.Text = val.ToString();
            double ms = val * 1000.0 / 48000.0;
            BufferLatency.Text = $"{val} frames (~{ms:F1}ms)";
            Native.Sidetone_SetBufferSize(val);
            RestartIfRunning();
        }

        private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ClickCount == 2)
            {
                return;
            }
            DragMove();
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void MinimizeButton_Click(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void MaximizeButton_Click(object sender, RoutedEventArgs e)
        {
            if (WindowState == WindowState.Maximized)
                WindowState = WindowState.Normal;
            else
                WindowState = WindowState.Maximized;
        }
    }
}
