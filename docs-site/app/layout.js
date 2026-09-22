import { Inter, Abyssinica_SIL } from "next/font/google";
import "./globals.css";

const inter = Inter({
  weight: ["400", "500", "700"],
  subsets: ["latin"],
  variable: "--font-body",
});

const abyssinicaSIL = Abyssinica_SIL({
  weight: "400",
  subsets: ["latin"],
  variable: "--font-brand",
});

export const metadata = {
  title: "LangBIOS Docs",
  description: "How to install, build, and use LangBIOS.",
};

export default function RootLayout({ children }) {
  return (
    <html lang="en" className={`${inter.variable} ${abyssinicaSIL.variable}`}>
      <body>{children}</body>
    </html>
  );
}
