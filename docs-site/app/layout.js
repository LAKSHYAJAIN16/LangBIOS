import "./globals.css";

export const metadata = {
  title: "LangBIOS Docs",
  description: "How to install, build, and use LangBIOS.",
};

export default function RootLayout({ children }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
