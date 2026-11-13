#include "icons.h"

#include <QAction>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QWidget>

#include "contrast.h"

namespace tuxdan {

namespace {

constexpr int kGrid = 20;
const char* const kIconProperty = "tuxdanIcon";

// The outlines: nearly black, or as light as the window in use needs them.
QColor ink()
{
    return readable(QColor(0x30, 0x30, 0x38), QApplication::palette().color(QPalette::Window), kGraphicContrast + 1.5);
}

const QColor kGreen(0x1E, 0x9E, 0x3A);
const QColor kBlue(0x2F, 0x6F, 0xD0);
const QColor kRed(0xC8, 0x30, 0x30);
const QColor kYellow(0xE8, 0xB0, 0x20);
const QColor kPaper(0xF4, 0xF4, 0xF0);

QPen outline(const QColor& colour = ink(), qreal width = 1.4)
{
    QPen pen(colour, width);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    return pen;
}

// The Dandanator's cartridge: a board with its edge connector below.
void drawCartridge(QPainter& p, const QColor& body)
{
    p.setPen(outline());
    p.setBrush(body);
    p.drawRoundedRect(QRectF(3, 2, 14, 13), 1.5, 1.5);
    p.setBrush(kPaper);
    p.drawRect(QRectF(5.5, 4.5, 9, 5));
    p.setBrush(kYellow);
    p.drawRect(QRectF(5, 15, 10, 3));
}

void drawArrow(QPainter& p, bool up, const QColor& colour)
{
    QPainterPath arrow;
    if (up) {
        arrow.moveTo(10, 3);
        arrow.lineTo(17, 10);
        arrow.lineTo(13, 10);
        arrow.lineTo(13, 17);
        arrow.lineTo(7, 17);
        arrow.lineTo(7, 10);
        arrow.lineTo(3, 10);
    } else {
        arrow.moveTo(10, 17);
        arrow.lineTo(17, 10);
        arrow.lineTo(13, 10);
        arrow.lineTo(13, 3);
        arrow.lineTo(7, 3);
        arrow.lineTo(7, 10);
        arrow.lineTo(3, 10);
    }
    arrow.closeSubpath();
    p.setPen(outline());
    p.setBrush(colour);
    p.drawPath(arrow);
}

void drawPlus(QPainter& p, QPointF at, qreal size)
{
    p.setPen(Qt::NoPen);
    p.setBrush(ink());
    p.drawEllipse(at, size + 1.2, size + 1.2);
    p.setPen(outline(kPaper, 1.6));
    p.drawLine(at - QPointF(size * 0.6, 0), at + QPointF(size * 0.6, 0));
    p.drawLine(at - QPointF(0, size * 0.6), at + QPointF(0, size * 0.6));
}

void draw(QPainter& p, Icon id)
{
    switch (id) {
    case Icon::Open:
        drawCartridge(p, kBlue);
        break;
    case Icon::Save:
        p.setPen(outline());
        p.setBrush(kBlue);
        p.drawRoundedRect(QRectF(3, 3, 14, 14), 1.5, 1.5);
        p.setBrush(kPaper);
        p.drawRect(QRectF(6, 3, 8, 5));
        p.drawRect(QRectF(5.5, 11, 9, 6));
        break;
    case Icon::Add:
        p.setPen(outline());
        p.setBrush(kPaper);
        p.drawRect(QRectF(4, 2, 10, 14));
        drawPlus(p, QPointF(14, 14), 3.5);
        break;
    case Icon::Playing:
        // A screen, the game on it.
        p.setPen(outline());
        p.setBrush(kPaper);
        p.drawRoundedRect(QRectF(2, 3, 15, 11), 1.5, 1.5);
        p.setBrush(kGreen);
        p.drawRect(QRectF(4.5, 5.5, 10, 6));
        drawPlus(p, QPointF(15, 15), 3.5);
        break;
    case Icon::Remove:
        p.setPen(outline(kRed, 2.6));
        p.drawLine(QPointF(5, 5), QPointF(15, 15));
        p.drawLine(QPointF(15, 5), QPointF(5, 15));
        break;
    case Icon::Up:
        drawArrow(p, true, kYellow);
        break;
    case Icon::Down:
        drawArrow(p, false, kYellow);
        break;
    case Icon::Export:
        p.setPen(outline());
        p.setBrush(kPaper);
        p.drawRect(QRectF(3, 2, 10, 14));
        p.setPen(outline(kGreen, 2.2));
        p.drawLine(QPointF(9, 12), QPointF(17, 12));
        p.drawLine(QPointF(14, 9), QPointF(17, 12));
        p.drawLine(QPointF(14, 15), QPointF(17, 12));
        break;
    case Icon::Try: {
        QPainterPath play;
        play.moveTo(5, 3);
        play.lineTo(17, 10);
        play.lineTo(5, 17);
        play.closeSubpath();
        p.setPen(outline());
        p.setBrush(kGreen);
        p.drawPath(play);
        break;
    }
    }
}

QPixmap picture(Icon id, int size, qreal ratio)
{
    QPixmap pixmap(QSize(size, size) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / double(kGrid), size / double(kGrid));
    draw(p, id);
    return pixmap;
}

}  // namespace

QIcon icon(Icon id)
{
    QIcon made;
    for (const int size : {16, 20, 24, 32, 40, 48})
        made.addPixmap(picture(id, size, 1.0));
    return made;
}

void setIcon(QAction* action, Icon id)
{
    action->setIcon(icon(id));
    action->setProperty(kIconProperty, static_cast<int>(id));
}

void refreshIcons(QWidget* window)
{
    for (QAction* action : window->findChildren<QAction*>()) {
        const QVariant id = action->property(kIconProperty);
        if (id.isValid())
            action->setIcon(icon(static_cast<Icon>(id.toInt())));
    }
}

QIcon cartridgeIcon()
{
    QIcon made;
    for (const int size : {16, 20, 24, 32, 40, 48}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(size / double(kGrid), size / double(kGrid));
        drawCartridge(p, ink());
        made.addPixmap(pixmap);
    }
    return made;
}

}  // namespace tuxdan
