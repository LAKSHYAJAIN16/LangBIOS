import { Abyssinica_SIL } from "next/font/google";
import "./globals.css";

const abyssinicaSIL = Abyssinica_SIL({
  weight: "400",
  subsets: ["latin"],
  variable: "--font-body",
});

export const metadata = {
  title: "LangBIOS Docs",
  description: "How to install, build, and use LangBIOS.",
};

export default function RootLayout({ children }) {
  return (
    <html lang="en" className={abyssinicaSIL.variable}>
      <body>{children}</body>
    </html>
  );
}
