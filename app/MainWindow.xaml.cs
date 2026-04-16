using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace SidetoneApp
{
    public partial class MainWindow : Window
    {
        private bool _initialized = false;
        private bool _loadingValues = false;

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

            if (Native.Sidetone_IsActive() == 0)
            {
                ToggleButton.Content = "Start";
            }

            _loadingValues = true;

            int gainVal = Native.Sidetone_GetGainPercent();
            int suppressVal = Native.Sidetone_GetSuppressionPercent();
            int inputGainVal = Native.Sidetone_GetInputGainPercent();

            GainSlider.Value = gainVal;
            GainValue.Text = gainVal + "%";

            SuppressSlider.Value = suppressVal;
            SuppressValue.Text = suppressVal + "%";

            InputGainSlider.Value = inputGainVal;
            InputGainValue.Text = inputGainVal + "%";

            _loadingValues = false;
            _initialized = true;
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
            }
            else
            {
                Native.Sidetone_Start();
                Native.Sidetone_SetActive(1);
                ToggleButton.Content = "Stop";
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
