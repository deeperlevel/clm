using System;

namespace CLA;

public readonly record struct Position(int X, int Y);
public readonly record struct Size(int Width, int Height);

// In C#, the static class is the canonical API and implementation entry point.
public static class Console
{
    private static readonly object StateLock = new();
    private static bool _initialized;
    private static bool? _cursorVisible;
    private static bool _cursorChanged;

    public static void Init()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void Restore()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void Clear()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void Flush()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void SetTitle(string title)
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static Size GetWindowSize()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void SetWindowSize(Size size)
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static Position? GetCursorPosition()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void SetCursorPosition(Position position)
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void ShowCursor()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void HideCursor()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static bool? IsCursorVisible()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static bool IsAttached()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void SetEchoOn()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void SetEchoOff()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void SetLineOn()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void SetLineOff()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void Write(string text)
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void WriteLine(string text)
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static string Read(string prompt)
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static string ReadLine()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static string ReadKey()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static string ReadCharacter()
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void WriteCharacter(string character)
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static bool Confirm(string prompt)
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void WriteColor(string text, string color)
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

    public static void WriteEffect(string text, double delay_seconds)
    {
        throw new NotImplementedException("CLA Console operation not implemented");
    }

}
