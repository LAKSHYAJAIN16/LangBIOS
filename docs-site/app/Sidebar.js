"use client";

import { useEffect, useState } from "react";

export default function Sidebar({ nav }) {
  const [activeId, setActiveId] = useState(nav[0]?.[0] ?? null);

  useEffect(() => {
    const sections = nav
      .map(([id]) => document.getElementById(id))
      .filter(Boolean);
    if (sections.length === 0) return;

    const observer = new IntersectionObserver(
      (entries) => {
        const visible = entries
          .filter((e) => e.isIntersecting)
          .sort((a, b) => a.boundingClientRect.top - b.boundingClientRect.top);
        if (visible.length > 0) {
          setActiveId(visible[0].target.id);
        }
      },
      { rootMargin: "-10% 0px -70% 0px", threshold: 0 }
    );

    sections.forEach((s) => observer.observe(s));
    return () => observer.disconnect();
  }, [nav]);

  return (
    <nav className="sidebar" aria-label="Table of contents">
      <div className="brand">LangBIOS</div>
      <ul className="navList">
        {nav.map(([id, label]) => (
          <li key={id}>
            <a href={`#${id}`} className={activeId === id ? "active" : undefined}>
              {label}
            </a>
          </li>
        ))}
      </ul>
      <a className="githubLink" href="https://github.com/LAKSHYAJAIN16/LangBIOS">
        GitHub &#8599;
      </a>
    </nav>
  );
}
