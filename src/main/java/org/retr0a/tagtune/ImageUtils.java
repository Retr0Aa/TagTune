package org.retr0a.tagtune;

import java.awt.*;
import java.awt.geom.RoundRectangle2D;
import java.awt.image.BufferedImage;

public class ImageUtils {
    public static BufferedImage scaleImage(BufferedImage original, int targetWidth, int targetHeight) {
        int type = (original.getTransparency() == Transparency.OPAQUE) ?
                BufferedImage.TYPE_INT_RGB : BufferedImage.TYPE_INT_ARGB;
        
        BufferedImage ret = original;
        int w = original.getWidth();
        int h = original.getHeight();

        do {
            if (w > targetWidth) {
                w /= 2;
                if (w < targetWidth) w = targetWidth;
            } else {
                w = targetWidth;
            }

            if (h > targetHeight) {
                h /= 2;
                if (h < targetHeight) h = targetHeight;
            } else {
                h = targetHeight;
            }

            BufferedImage tmp = new BufferedImage(w, h, type);
            Graphics2D g2 = tmp.createGraphics();
            g2.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BICUBIC);
            g2.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);
            g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
            g2.drawImage(ret, 0, 0, w, h, null);
            g2.dispose();

            ret = tmp;
        } while (w != targetWidth || h != targetHeight);

        return ret;
    }

    public static Image applyMacEffects(Image sourceIcon) {
        int size = 1024;
        BufferedImage result = new BufferedImage(size, size, BufferedImage.TYPE_INT_ARGB);
        Graphics2D g2 = result.createGraphics();
        
        g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        g2.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);

        // 1. Padding: Native macOS icons are ~80% of the full image size to leave room for shadows/breathing
        int contentSize = (int) (size * 0.82);
        int offset = (size - contentSize) / 2;
        int arc = (int) (contentSize * 0.45); // Standard squircle-like radius

        // 2. Clip to Squircle
        Shape squircle = new RoundRectangle2D.Float(offset, offset, contentSize, contentSize, arc, arc);
        g2.setClip(squircle);
        
        // Draw the source image
        g2.drawImage(sourceIcon, offset, offset, contentSize, contentSize, null);

        // 3. "Liquid Glass" effect: Subtle top-to-bottom gloss
        g2.setClip(null); // Remove clip for the overlay
        
        // Outer shadow/border for depth
        g2.setColor(new Color(0, 0, 0, 40));
        g2.setStroke(new BasicStroke(2));
        g2.draw(squircle);

        // Glassy overlay (top half highlight)
        GradientPaint glass = new GradientPaint(
            0, offset, new Color(255, 255, 255, 60),
            0, offset + (contentSize / 2), new Color(255, 255, 255, 0)
        );
        g2.setPaint(glass);
        g2.fill(squircle);

        g2.dispose();
        return result;
    }

    public static Image createAppIcon(int size) {
        BufferedImage image = new BufferedImage(size, size, BufferedImage.TYPE_INT_ARGB);
        Graphics2D g2 = image.createGraphics();
        g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        
        int arc = (int)(size * 0.30);
        g2.setColor(new Color(30, 144, 255));
        g2.fillRoundRect(20, 20, size-40, size-40, arc, arc);

        g2.setColor(Color.WHITE);
        g2.setFont(new Font("SansSerif", Font.BOLD, (int)(size * 0.4)));
        FontMetrics fm = g2.getFontMetrics();
        String text = "TT";
        int x = (size - fm.stringWidth(text)) / 2;
        int y = ((size - fm.getHeight()) / 2) + fm.getAscent();
        g2.drawString(text, x, y);

        g2.dispose();
        return image;
    }
}
