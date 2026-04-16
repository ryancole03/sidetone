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
    }
}
