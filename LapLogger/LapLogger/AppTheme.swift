import SwiftUI

enum AppTheme {
    static let background = Color(red: 0.045, green: 0.055, blue: 0.07)
    static let card = Color(red: 0.10, green: 0.12, blue: 0.15)
    static let cardSecondary = Color(red: 0.14, green: 0.16, blue: 0.20)
    static let accent = Color(red: 0.20, green: 0.57, blue: 1.0)
    static let healthy = Color(red: 0.18, green: 0.80, blue: 0.48)
    static let warning = Color.orange
    static let danger = Color(red: 1.0, green: 0.31, blue: 0.27)
}

struct DashboardCard<Content: View>: View {
    @ViewBuilder var content: Content

    var body: some View {
        content
            .padding(16)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(AppTheme.card, in: RoundedRectangle(cornerRadius: 18, style: .continuous))
    }
}

struct ConnectionPill: View {
    let connected: Bool
    let title: String

    var body: some View {
        Label(title, systemImage: connected ? "dot.radiowaves.left.and.right" : "bolt.horizontal.circle")
            .font(.caption.weight(.bold))
            .foregroundStyle(connected ? AppTheme.healthy : .secondary)
            .padding(.horizontal, 10)
            .padding(.vertical, 7)
            .background((connected ? AppTheme.healthy : Color.secondary).opacity(0.14), in: Capsule())
    }
}

struct MetricCard: View {
    let title: String
    let symbol: String
    let value: String
    let detail: String

    var body: some View {
        DashboardCard {
            VStack(alignment: .leading, spacing: 11) {
                Image(systemName: symbol).foregroundStyle(AppTheme.accent)
                Text(title.uppercased()).font(.caption2.weight(.bold)).foregroundStyle(.secondary)
                Text(value).font(.title3.weight(.semibold)).foregroundStyle(.white)
                Text(detail).font(.caption2).foregroundStyle(.secondary)
            }
        }
    }
}

struct ActivityRow: View {
    let event: ActivityEvent

    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            Image(systemName: event.kind.symbolName)
                .foregroundStyle(color)
                .frame(width: 22)
            VStack(alignment: .leading, spacing: 4) {
                Text(event.message).font(.subheadline.weight(.medium)).foregroundStyle(.white)
                Text(event.date, format: .dateTime.hour().minute().second())
                    .font(.caption).foregroundStyle(.secondary)
            }
            Spacer(minLength: 0)
            Text(event.kind.rawValue.uppercased()).font(.caption2.weight(.bold)).foregroundStyle(.secondary)
        }
        .padding(.vertical, 4)
    }

    private var color: Color {
        switch event.kind {
        case .status: AppTheme.accent
        case .command: AppTheme.healthy
        case .connection: AppTheme.healthy
        case .error: AppTheme.danger
        }
    }
}
