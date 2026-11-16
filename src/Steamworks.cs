// Minimal replacement for the Steamworks.NET 10.0.0 assembly that Celeste.exe
// references. The original went through libCSteamworks (x86 only, old SDK);
// this one exposes the same public API for the handful of calls Celeste makes
// and forwards them to the flat C API of a current, arm64-capable
// libsteam_api.dylib.
using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

[assembly: AssemblyTitle("Steamworks.NET")]
[assembly: AssemblyDescription("Celeste arm64 compatibility layer over the Steamworks flat API")]
[assembly: AssemblyVersion("10.0.0.0")]

namespace Steamworks
{
	public struct AppId_t
	{
		public uint m_AppId;
		public AppId_t(uint value) { m_AppId = value; }
		public override string ToString() { return m_AppId.ToString(); }
	}

	public struct SteamAPICall_t
	{
		public ulong m_SteamAPICall;
		public SteamAPICall_t(ulong value) { m_SteamAPICall = value; }
	}

	internal static class Native
	{
		private const string Lib = "steam_api";
		private const CallingConvention CC = CallingConvention.Cdecl;

		[DllImport(Lib, CallingConvention = CC)]
		[return: MarshalAs(UnmanagedType.I1)]
		public static extern bool SteamAPI_RestartAppIfNecessary(uint unOwnAppID);

		// Returns ESteamAPIInitResult (0 == OK); errMsg is a char[1024] buffer.
		[DllImport(Lib, CallingConvention = CC)]
		public static extern int SteamAPI_InitFlat(IntPtr errMsg);

		[DllImport(Lib, CallingConvention = CC)]
		public static extern void SteamAPI_RunCallbacks();

		[DllImport(Lib, CallingConvention = CC)]
		public static extern IntPtr SteamAPI_SteamApps_v009();

		[DllImport(Lib, CallingConvention = CC)]
		public static extern IntPtr SteamAPI_ISteamApps_GetCurrentGameLanguage(IntPtr self);

		[DllImport(Lib, CallingConvention = CC)]
		public static extern IntPtr SteamAPI_SteamUserStats_v013();

		[DllImport(Lib, CallingConvention = CC)]
		[return: MarshalAs(UnmanagedType.I1)]
		public static extern bool SteamAPI_ISteamUserStats_GetStatInt32(IntPtr self, byte[] name, out int data);

		[DllImport(Lib, CallingConvention = CC)]
		[return: MarshalAs(UnmanagedType.I1)]
		public static extern bool SteamAPI_ISteamUserStats_SetStatInt32(IntPtr self, byte[] name, int data);

		[DllImport(Lib, CallingConvention = CC)]
		[return: MarshalAs(UnmanagedType.I1)]
		public static extern bool SteamAPI_ISteamUserStats_GetAchievement(IntPtr self, byte[] name, [MarshalAs(UnmanagedType.I1)] out bool achieved);

		[DllImport(Lib, CallingConvention = CC)]
		[return: MarshalAs(UnmanagedType.I1)]
		public static extern bool SteamAPI_ISteamUserStats_SetAchievement(IntPtr self, byte[] name);

		[DllImport(Lib, CallingConvention = CC)]
		[return: MarshalAs(UnmanagedType.I1)]
		public static extern bool SteamAPI_ISteamUserStats_StoreStats(IntPtr self);

		[DllImport(Lib, CallingConvention = CC)]
		public static extern ulong SteamAPI_ISteamUserStats_RequestGlobalStats(IntPtr self, int historyDays);

		[DllImport(Lib, CallingConvention = CC)]
		[return: MarshalAs(UnmanagedType.I1)]
		public static extern bool SteamAPI_ISteamUserStats_GetGlobalStatInt64(IntPtr self, byte[] name, out long data);

		public static byte[] Utf8(string s)
		{
			if (s == null) s = string.Empty;
			byte[] bytes = new byte[Encoding.UTF8.GetByteCount(s) + 1];
			Encoding.UTF8.GetBytes(s, 0, s.Length, bytes, 0);
			return bytes;
		}

		public static string FromUtf8(IntPtr ptr)
		{
			if (ptr == IntPtr.Zero) return null;
			int len = 0;
			while (Marshal.ReadByte(ptr, len) != 0) len++;
			byte[] bytes = new byte[len];
			Marshal.Copy(ptr, bytes, 0, len);
			return Encoding.UTF8.GetString(bytes);
		}
	}

	public static class SteamAPI
	{
		internal static bool Initialized;

		public static bool RestartAppIfNecessary(AppId_t unOwnAppID)
		{
			return Native.SteamAPI_RestartAppIfNecessary(unOwnAppID.m_AppId);
		}

		public static bool Init()
		{
			IntPtr err = Marshal.AllocHGlobal(1024);
			try
			{
				Marshal.WriteByte(err, 0, 0);
				int result = Native.SteamAPI_InitFlat(err);
				if (result != 0)
				{
					Console.Error.WriteLine("[Steamworks] SteamAPI_Init failed (" + result + "): " + Native.FromUtf8(err));
				}
				Initialized = result == 0;
				return Initialized;
			}
			finally
			{
				Marshal.FreeHGlobal(err);
			}
		}

		public static void RunCallbacks()
		{
			if (Initialized) Native.SteamAPI_RunCallbacks();
		}
	}

	public static class SteamApps
	{
		public static string GetCurrentGameLanguage()
		{
			return Native.FromUtf8(Native.SteamAPI_ISteamApps_GetCurrentGameLanguage(Native.SteamAPI_SteamApps_v009()));
		}
	}

	public static class SteamUserStats
	{
		private static IntPtr Self { get { return Native.SteamAPI_SteamUserStats_v013(); } }

		// Removed from the Steamworks SDK in 1.61: the Steam client now loads the
		// current user's stats before the game starts, so this always succeeds.
		public static bool RequestCurrentStats()
		{
			return SteamAPI.Initialized;
		}

		public static SteamAPICall_t RequestGlobalStats(int nHistoryDays)
		{
			return new SteamAPICall_t(Native.SteamAPI_ISteamUserStats_RequestGlobalStats(Self, nHistoryDays));
		}

		public static bool GetStat(string pchName, out int pData)
		{
			return Native.SteamAPI_ISteamUserStats_GetStatInt32(Self, Native.Utf8(pchName), out pData);
		}

		public static bool SetStat(string pchName, int nData)
		{
			return Native.SteamAPI_ISteamUserStats_SetStatInt32(Self, Native.Utf8(pchName), nData);
		}

		public static bool GetGlobalStat(string pchStatName, out long pData)
		{
			return Native.SteamAPI_ISteamUserStats_GetGlobalStatInt64(Self, Native.Utf8(pchStatName), out pData);
		}

		public static bool GetAchievement(string pchName, out bool pbAchieved)
		{
			return Native.SteamAPI_ISteamUserStats_GetAchievement(Self, Native.Utf8(pchName), out pbAchieved);
		}

		public static bool SetAchievement(string pchName)
		{
			return Native.SteamAPI_ISteamUserStats_SetAchievement(Self, Native.Utf8(pchName));
		}

		public static bool StoreStats()
		{
			return Native.SteamAPI_ISteamUserStats_StoreStats(Self);
		}
	}
}
