using System;
using System.Runtime.InteropServices;

namespace SidetoneApp
{
    internal static class Native
    {
        private const string DllName = "NativeLib.dll";

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Sidetone_Init();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Sidetone_Shutdown();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Sidetone_Start();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Sidetone_Stop();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Sidetone_SetGainPercent(int percent);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Sidetone_SetSuppressionPercent(int percent);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Sidetone_SetInputGainPercent(int percent);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Sidetone_IsActive();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Sidetone_SetActive(int active);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Sidetone_GetGainPercent();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Sidetone_GetSuppressionPercent();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Sidetone_GetInputGainPercent();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Sidetone_GetInputDeviceCount();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern int Sidetone_GetInputDevice(int index, [Out] char[] id, [Out] char[] name, int bufferSize);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Sidetone_GetOutputDeviceCount();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern int Sidetone_GetOutputDevice(int index, [Out] char[] id, [Out] char[] name, int bufferSize);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern void Sidetone_SetInputDevice(string deviceId);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern void Sidetone_SetOutputDevice(string deviceId);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern void Sidetone_GetCurrentInputDevice([Out] char[] deviceId, int bufferSize);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern void Sidetone_GetCurrentOutputDevice([Out] char[] deviceId, int bufferSize);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Sidetone_SetNoiseGateThreshold(int threshold);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Sidetone_GetNoiseGateThreshold();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void Sidetone_SetBufferSize(int size);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Sidetone_GetBufferSize();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Sidetone_GetInputSampleRate();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int Sidetone_GetOutputSampleRate();
    }
}
