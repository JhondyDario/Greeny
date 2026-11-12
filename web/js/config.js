// Datos del proyecto de Supabase. Son publicos por diseño: lo que protege los
// datos son las reglas de seguridad por fila de la base de datos.
window.GREENY_CONFIG = {
  supabaseUrl: "https://mlwzlirkjgpyyldbtoci.supabase.co",
  supabaseKey: "sb_publishable_r4Y8LjTEzu7A2Z9ecr2hEw_Jr-OT8RC",
  repoUrl: "https://github.com/JhondyDario/Greeny",
  donateUrl: "https://www.paypal.com/donate/?hosted_button_id=B82PT4TEHBG3G",
  // Clave del SITIO de Cloudflare Turnstile (publica). Vacia = sin captcha.
  // La clave SECRETA va solo en Supabase (Authentication → Attack Protection).
  turnstileSiteKey: "0x4AAAAAAF-38a9egevH7JCS",
};
